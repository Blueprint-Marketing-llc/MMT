import json as js
import logging
import multiprocessing
import os
import subprocess
import tempfile
import time

import requests

import cli
from cli import mmt_javamain, IllegalStateException, IllegalArgumentException
from cli.libs import fileutils, daemon, shell
from cli.mt import BilingualCorpus
from cli.mt.processing import TrainingPreprocessor, Tokenizer

__author__ = 'Davide Caroselli'

DEFAULT_MMT_API_PORT = 8045
DEFAULT_MMT_CLUSTER_PORTS = [5016, 5017, 5018, 5019]


class MMTApi:
    DEFAULT_TIMEOUT = 60 * 60  # sec

    def __init__(self, host="localhost", port=None):
        if host[:7] == "http://":
            host = host[7:]
        if host.find(":") != -1:
            host, port = host.split(":")
        port = port if port else DEFAULT_MMT_API_PORT
        self.port = int(port)
        self.host = host
        self._url_template = 'http://{host}:{port}/{endpoint}'

        logging.getLogger('requests').setLevel(1000)
        logging.getLogger('urllib3').setLevel(1000)

    @staticmethod
    def _unpack(r):
        if r.status_code != requests.codes.ok:
            raise Exception('HTTP request failed with code ' + str(r.status_code) + ': ' + r.url)
        content = r.json()

        return content['data'] if 'data' in content else None

    def _get(self, endpoint, params=None):
        url = self._url_template.format(host=self.host, port=self.port, endpoint=endpoint)
        r = requests.get(url, params=params, timeout=MMTApi.DEFAULT_TIMEOUT)
        return self._unpack(r)

    def _delete(self, endpoint):
        url = self._url_template.format(host=self.host, port=self.port, endpoint=endpoint)
        r = requests.delete(url, timeout=MMTApi.DEFAULT_TIMEOUT)
        return self._unpack(r)

    def _put(self, endpoint, json=None, params=None):
        url = self._url_template.format(host=self.host, port=self.port, endpoint=endpoint)

        data = headers = None
        if json is not None:
            data = js.dumps(json)
            headers = {'Content-type': 'application/json'}
        elif params is not None:
            data = params

        r = requests.put(url, data=data, headers=headers, timeout=MMTApi.DEFAULT_TIMEOUT)
        return self._unpack(r)

    def _post(self, endpoint, json=None, params=None):
        url = self._url_template.format(host=self.host, port=self.port, endpoint=endpoint)

        data = headers = None
        if json is not None:
            data = js.dumps(json)
            headers = {'Content-type': 'application/json'}
        elif params is not None:
            data = params

        r = requests.post(url, data=data, headers=headers, timeout=MMTApi.DEFAULT_TIMEOUT)
        return self._unpack(r)

    @staticmethod
    def _encode_context(context):
        return ','.join([('%d:%f' % (
            el['domain']['id'] if isinstance(el['domain'], dict) else el['domain'],
            el['score'])
                          ) for el in context])

    def stats(self):
        return self._get('_stat')

    def update_features(self, features):
        return self._put('decoder/features', json=features)

    def get_features(self):
        return self._get('decoder/features')

    def get_context_f(self, document, limit=None):
        params = {'local_file': document}
        if limit is not None:
            params['limit'] = limit
        return self._get('context', params=params)

    def get_context_s(self, text, limit=None):
        params = {'text': text}
        if limit is not None:
            params['limit'] = limit
        return self._get('context', params=params)

    def create_session(self, context):
        return self._post('sessions', params={
            'context_weights': self._encode_context(context)
        })

    def close_session(self, session):
        return self._delete('sessions/' + str(session))

    def translate(self, source, session=None, context=None, nbest=None):
        p = {'q': source}
        if session is not None:
            p['session'] = session
        if nbest is not None:
            p['nbest'] = nbest
        if context is not None:
            p['context_weights'] = self._encode_context(context)

        return self._get('translate', params=p)

    def create_domain(self, name):
        params = {'name': name}
        return self._post('domains', params=params)

    def append_to_domain(self, domain, source, target):
        params = {'source': source, 'target': target}
        return self._put('domains/' + str(domain), params=params)

    def import_into_domain(self, domain, tmx):
        params = {
            'domain': domain,
            'content_type' : 'tmx',
            'local_file' : tmx
        }

        return self._post('imports', params=params)

    def get_all_domains(self):
        return self._get('domains')


class _tuning_logger:
    def __init__(self, count, line_len=70):
        self.line_len = line_len
        self.count = count
        self._current_step = 0
        self._step = None
        self._api_port = None

    def start(self, node, corpora):
        engine = node.engine
        self._api_port = node.api.port

        print '\n============ TUNING STARTED ============\n'
        print 'ENGINE:  %s' % engine.name
        print 'CORPORA: %s (%d documents)' % (corpora[0].get_folder(), len(corpora))
        print 'LANGS:   %s > %s' % (engine.source_lang, engine.target_lang)
        print

    def step(self, step):
        self._step = step
        self._current_step += 1
        return self

    def completed(self, bleu):
        print '\n============ TUNING SUCCESS ============\n'
        print '\nFinal BLEU: %.2f\n' % (bleu * 100.)
        print 'You can try the API with:'
        print '\tcurl "http://localhost:{port}/translate?q=hello+world&context=computer"'.format(port=self._api_port)
        print

    def __enter__(self):
        message = 'INFO: (%d of %d) %s... ' % (self._current_step, self.count, self._step)
        print message.ljust(self.line_len),

        self._start_time = time.time()
        return self

    def __exit__(self, *_):
        self._end_time = time.time()
        print 'DONE (in %ds)' % int(self._end_time - self._start_time)


class EmbeddedKafka:
    def __init__(self, engine):
        self._engine = engine
        self._log_file = None

        self._data = os.path.join(engine.models_path, 'kafka')
        self._pidfile = os.path.join(engine.get_runtime_path(), 'kafka.pid')

        self._zookeeper_bin = os.path.join(cli.VENDOR_DIR, 'kafka-0.10.0.1', 'bin', 'zookeeper-server-start.sh')
        self._kafka_bin = os.path.join(cli.VENDOR_DIR, 'kafka-0.10.0.1', 'bin', 'kafka-server-start.sh')

    def _get_pids(self):
        kpid, zpid = 0, 0
        if os.path.isfile(self._pidfile):
            with open(self._pidfile) as pid_file:
                _kpid, _zpid = pid_file.read().split(' ', 2)
                kpid, zpid = int(_kpid), int(_zpid)

        return kpid, zpid

    def _set_pids(self, kpid, zpid):
        parent_dir = os.path.abspath(os.path.join(self._pidfile, os.pardir))
        if not os.path.isdir(parent_dir):
            fileutils.makedirs(parent_dir, exist_ok=True)

        with open(self._pidfile, 'w') as pid_file:
            pid_file.write(str(kpid) + ' ' + str(zpid))

    def is_running(self):
        kpid, _ = self._get_pids()

        if kpid == 0:
            return False

        return daemon.is_running(kpid)

    def stop(self):
        kpid, zpid = self._get_pids()

        if not self.is_running():
            raise IllegalStateException('process is not running')

        daemon.kill(kpid, 5)
        daemon.kill(zpid)

    def start(self):
        if self.is_running():
            raise IllegalStateException('process is already running')

        self._log_file = self._engine.get_logfile('embedded-kafka', ensure=True)

        success = False
        zpid, kpid = 0, 0

        log = open(self._log_file, 'w')

        try:
            zpid = self._start_zookeeper(log)
            if zpid is None:
                raise IllegalStateException(
                    'failed to start zookeeper, check log file for more details: ' + self._log_file)

            kpid = self._start_kafka(log)
            if kpid is None:
                raise IllegalStateException(
                    'failed to start kafka, check log file for more details: ' + self._log_file)

            self._set_pids(kpid, zpid)

            success = True
        except:
            if not success:
                daemon.kill(kpid)
                daemon.kill(zpid)
                log.close()
            raise

    def _start_zookeeper(self, log, port=2181):
        zdata = os.path.abspath(os.path.join(self._data, 'zdata'))
        if not os.path.isdir(zdata):
            fileutils.makedirs(zdata, exist_ok=True)

        config = os.path.join(self._data, 'zookeeper.properties')
        with open(config, 'w') as cout:
            cout.write('dataDir={data}\n'.format(data=zdata))
            cout.write('clientPort={port}\n'.format(port=port))
            cout.write('maxClientCnxns=0\n')

        command = [self._zookeeper_bin, config]
        zookeeper = subprocess.Popen(command, stdout=log, stderr=log, shell=False).pid

        for i in range(1, 5):
            try:
                msg = fileutils.netcat('127.0.0.1', port, 'ruok', timeout=2)
            except:
                msg = None

            if 'imok' == msg:
                return zookeeper
            else:
                time.sleep(1)

        daemon.kill(zookeeper)
        return None

    def _start_kafka(self, log, port=9092, zookeeper_port=2181):
        kdata = os.path.abspath(os.path.join(self._data, 'kdata'))
        if not os.path.isdir(kdata):
            fileutils.makedirs(kdata, exist_ok=True)

        config = os.path.join(self._data, 'kafka.properties')
        with open(config, 'w') as cout:
            cout.write('broker.id=0\n')
            cout.write('listeners=PLAINTEXT://0.0.0.0:{port}\n'.format(port=port))
            cout.write('log.dirs={data}\n'.format(data=kdata))
            cout.write('num.partitions=1\n')
            cout.write('log.retention.hours=8760000\n')
            cout.write('zookeeper.connect=localhost:{port}\n'.format(port=zookeeper_port))

        command = [self._kafka_bin, config]
        kafka = subprocess.Popen(command, stdout=log, stderr=log, shell=False).pid

        for i in range(1, 5):
            with open(log.name, 'r') as rlog:
                for line in rlog:
                    if 'INFO [Kafka Server 0], started (kafka.server.KafkaServer)' in line:
                        return kafka

            time.sleep(1)

        daemon.kill(kafka)
        return None


class ClusterNode(object):
    __SIGTERM_TIMEOUT = 10  # after this amount of seconds, there is no excuse for a process to still be there.
    __LOG_FILENAME = 'node'

    STATUS = {
        'NONE': 0,

        'CREATED': 100,
        'JOINING': 200,
        'JOINED': 300,
        'SYNCHRONIZING': 400,
        'SYNCHRONIZED': 500,
        'LOADING': 600,
        'LOADED': 700,
        'UPDATING': 800,
        'UPDATED': 900,
        'READY': 1000,
        'SHUTDOWN': 1100,
        'TERMINATED': 1200,

        'ERROR': 9999,
    }

    def __init__(self, engine, rest=True, api_port=None, cluster_ports=None,
                 sibling=None, verbosity=None):
        self.engine = engine
        self.api = MMTApi(port=api_port)

        self._kafka = EmbeddedKafka(engine) if sibling is None else None

        self._pidfile = os.path.join(engine.get_runtime_path(), 'node.pid')

        self._cluster_ports = cluster_ports if cluster_ports is not None else DEFAULT_MMT_CLUSTER_PORTS
        self._api_port = api_port if api_port is not None else DEFAULT_MMT_API_PORT
        self._start_rest_server = rest
        self._sibling = sibling
        self._verbosity = verbosity
        self._status_file = os.path.join(engine.get_runtime_path(), 'node.status')
        self._log_file = engine.get_logfile(ClusterNode.__LOG_FILENAME, ensure=False)

        self._mert_script = os.path.join(cli.PYOPT_DIR, 'mert-moses.perl')
        self._mert_i_script = os.path.join(cli.PYOPT_DIR, 'mertinterface.py')

    def _get_pid(self):
        pid = 0
        if os.path.isfile(self._pidfile):
            with open(self._pidfile) as pid_file:
                pid = int(pid_file.read())

        return pid

    def _set_pid(self, pid):
        parent_dir = os.path.abspath(os.path.join(self._pidfile, os.pardir))
        if not os.path.isdir(parent_dir):
            fileutils.makedirs(parent_dir, exist_ok=True)

        with open(self._pidfile, 'w') as pid_file:
            pid_file.write(str(pid))

    def is_running(self):
        pid = self._get_pid()

        if pid == 0:
            return False

        return daemon.is_running(pid)

    def stop(self):
        pid = self._get_pid()

        if not self.is_running():
            raise IllegalStateException('process is not running')

        daemon.kill(pid, ClusterNode.__SIGTERM_TIMEOUT)
        if self._kafka:
            self._kafka.stop()

    def start(self):
        if self.is_running():
            raise IllegalStateException('process is already running')

        if self._kafka:
            self._kafka.start()

        success = False
        process = self._start_process()
        pid = process.pid

        if pid > 0:
            self._set_pid(pid)

            for _ in range(0, 5):
                success = self.is_running()
                if success:
                    break

                time.sleep(1)

        if not success:
            if self._kafka:
                self._kafka.stop()
            raise Exception('failed to start node, check log file for more details: ' + self._log_file)

    def _start_process(self):
        if not os.path.isdir(self.engine.get_runtime_path()):
            fileutils.makedirs(self.engine.get_runtime_path(), exist_ok=True)
        logs_folder = os.path.abspath(os.path.join(self._log_file, os.pardir))

        args = ['-e', self.engine.name, '-p', str(self._cluster_ports[0]), str(self._cluster_ports[1]),
                '--status-file', self._status_file, '--logs', logs_folder]

        if self._start_rest_server:
            args.append('-a')
            args.append(str(self._api_port))

        if self._verbosity is not None:
            args.append('-v')
            args.append(str(self._verbosity))

        if self._sibling is not None:
            args.append('--member')
            args.append(str(self._sibling))

        command = mmt_javamain('eu.modernmt.cli.ClusterNodeMain', args, hserr_path=logs_folder)

        if os.path.isfile(self._status_file):
            os.remove(self._status_file)

        return subprocess.Popen(command, stdout=open('/home/ubuntu/test.log', 'w'), stderr=subprocess.STDOUT, shell=False)

    def get_state(self):
        state = None

        if os.path.isfile(self._status_file) and self.is_running():
            state = {}

            with open(self._status_file) as properties:
                for line in properties:
                    line = line.strip()

                    if not line.startswith('#'):
                        (key, value) = line.split('=', 2)
                        state[key] = value

        return state

    def _get_status(self):
        status = 'NONE'
        state = self.get_state()
        if state is not None and 'status' in state:
            status = state['status']

        return ClusterNode.STATUS[status] if status in ClusterNode.STATUS else ClusterNode.STATUS['NONE']

    def wait(self, status):
        status = ClusterNode.STATUS[status]
        current = self._get_status()

        while current < status:
            time.sleep(1)
            current = self._get_status() if self.is_running() else ClusterNode.STATUS['ERROR']

        if current == ClusterNode.STATUS['ERROR']:
            raise Exception('failed to start node, check log file for more details: ' + self._log_file)

    def tune(self, corpora=None, debug=False, context_enabled=True, random_seeds=False, max_iterations=25):
        if corpora is None:
            corpora = BilingualCorpus.list(os.path.join(self.engine.data_path, TrainingPreprocessor.DEV_FOLDER_NAME))

        if len(corpora) == 0:
            raise IllegalArgumentException('empty corpora')

        if not self.is_running():
            raise IllegalStateException('No MMT Server running, start the engine first')

        tokenizer = Tokenizer()

        target_lang = self.engine.target_lang
        source_lang = self.engine.source_lang

        source_corpora = [BilingualCorpus.make_parallel(corpus.name, corpus.get_folder(), [source_lang])
                          for corpus in corpora]
        reference_corpora = [BilingualCorpus.make_parallel(corpus.name, corpus.get_folder(), [target_lang])
                             for corpus in corpora]

        cmdlogger = _tuning_logger(4)
        cmdlogger.start(self, corpora)

        working_dir = self.engine.get_tempdir('tuning')
        mert_wd = os.path.join(working_dir, 'mert')

        try:
            # Tokenization
            tokenized_output = os.path.join(working_dir, 'reference_corpora')
            fileutils.makedirs(tokenized_output, exist_ok=True)

            with cmdlogger.step('Corpora tokenization') as _:
                reference_corpora = tokenizer.process_corpora(reference_corpora, tokenized_output)

            # Create merged corpus
            with cmdlogger.step('Merging corpus') as _:
                # source
                source_merged_corpus = os.path.join(working_dir, 'corpus.' + source_lang)

                with open(source_merged_corpus, 'wb') as out:
                    for corpus in source_corpora:
                        out.write(corpus.get_file(source_lang) + '\n')

                # target
                target_merged_corpus = os.path.join(working_dir, 'corpus.' + target_lang)
                fileutils.merge([corpus.get_file(target_lang) for corpus in reference_corpora], target_merged_corpus)

            # Run MERT algorithm
            with cmdlogger.step('Tuning') as _:
                # Start MERT
                decoder_flags = ['--port', str(self.api.port)]

                if not context_enabled:
                    decoder_flags.append('--skip-context-analysis')
                    decoder_flags.append('1')

                fileutils.makedirs(mert_wd, exist_ok=True)

                with tempfile.NamedTemporaryFile() as runtime_moses_ini:
                    #
                    # Reasoning for MERT options: "-t random-direction -m 16"
                    # (see https://github.com/ModernMT/MMT/issues/209 "MERT weights going to all-zero when tuning with moses2 (except for DM0_2 = 1.0)")
                    #
                    # When using moses2, MERT is hitting these weights in iteration 7 with the toy data from examples/data/train:
                    # 
                    # "The decoder died. CONFIG WAS -weight-overwrite 'PhrasePenalty0= 0.000000 # Sapt= 0.000000 0.000000 0.000000 0.000000 InterpolatedLM= 0.000000 # Distortion0= 0.000000 WordPenalty0= -0.000000 DM0= 0.000000 -0.000000 1.000000 0.000000 0.000000 0.000000 0.000000 0.000000'"
                    # 
                    # Two sentences in an n-best list have almost exactly the same feature value, not exactly but ~ up to 32-bit float machine precision (epsilon).
                    # The Powell search algo in MERT is looking for changing line slopes along a feature weight in `Optimizer::LineOptimize()`. The intersection point of two lines with almost exactly the same # slope lies very far away in `intersect()` and unfortunately that's a sentence on the border having the best BLEU.
                    # There's no reason not to have n-best sentences with the same feature weight (note the reordering statistics in that DM0_2 feature [DiscontinuousLeftOrientation] are rational numbers).
                    # 
                    # '-t random-direction' avoids going in straight axis directions, and always goes in a random direction instead.
                    # 16 random directions, because the classic Powell search also does 16 directions (in the direction of each feature value, of which there are 16).
                    # 
                    # so, we use the following MERT options: "-t random-direction -m 16"
                    #
                    command = [self._mert_script, source_merged_corpus, target_merged_corpus,
                               self._mert_i_script, runtime_moses_ini.name, '--threads',
                               str(multiprocessing.cpu_count()), '--mertdir', cli.BIN_DIR,
                               '--mertargs', '\'--binary -t random-direction -m 16 --sctype BLEU\'', '--working-dir', mert_wd, '--nbest', '100',
                               '--decoder-flags', '"' + ' '.join(decoder_flags) + '"', '--nonorm', '--closest',
                               '--no-filter-phrase-table']

                    if not random_seeds:
                        command.append('--predictable-seeds')
                    if max_iterations > 0:
                        command.append('--maximum-iterations={num}'.format(num=max_iterations))

                    with open(self.engine.get_logfile('mert'), 'wb') as log:
                        shell.execute(' '.join(command), stdout=log, stderr=log)

            # Read optimized configuration
            with cmdlogger.step('Applying changes') as _:
                bleu_score = 0
                weights = {}
                found_weights = False

                with open(os.path.join(mert_wd, 'moses.ini')) as moses_ini:
                    for line in moses_ini:
                        line = line.strip()

                        if len(line) == 0:
                            continue
                        elif found_weights:
                            tokens = line.split()
                            weights[tokens[0].rstrip('=')] = [float(val) for val in tokens[1:]]
                        elif line.startswith('# BLEU'):
                            bleu_score = float(line.split()[2])
                        elif line == '[weight]':
                            found_weights = True

                _ = self.api.update_features(weights)

            cmdlogger.completed(bleu_score)
        finally:
            if not debug:
                self.engine.clear_tempdir()

    def new_domain_from_tmx(self, tmx, name=None):
        if name is None:
            name = os.path.basename(os.path.splitext(tmx)[0])

        domain = self.api.create_domain(name)
        self.api.import_into_domain(domain['id'], tmx)

        return domain

    def append_to_domain(self, domain, source, target):
        try:
            domain = int(domain)
        except ValueError:
            domains = self.api.get_all_domains()
            ids = [d['id'] for d in domains if d['name'] == domain]

            if len(ids) == 0:
                raise IllegalArgumentException('unable to find domain "' + domain + '"')
            elif len(ids) > 1:
                raise IllegalArgumentException(
                    'ambiguous domain name "' + domain + '", choose one of the following ids: ' + str(ids))
            else:
                domain = ids[0]

        return self.api.append_to_domain(domain, source, target)

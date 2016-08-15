import argparse
import logging
import os
import sys

import requests

from cli.cluster import MMTApi, DEFAULT_MMT_API_PORT
from cli.libs import multithread


class CLIArgsException(Exception):
    def __init__(self, error):
        self.message = error


class Executor:
    def __init__(self, api, args):
        self.api = api
        self.processing = args.process
        self.nbest = args.nbest

        self.nbest_out = sys.stdout
        self._nbest_file = None
        if args.nbest_file is not None:
            self.nbest_out = self._nbest_file = open(args.nbest_file, 'wb')

        context = None

        if args.context is not None:
            context = api.get_context_s(args.context)
        elif args.context_file is not None:
            context = api.get_context_f(args.context_file)
        elif args.context_weights is not None:
            context = self.__parse_context_map(args.context_weights)

        if context is not None:
            self.session = api.create_session(context)['id']
        else:
            self.session = None

    @staticmethod
    def __parse_context_map(text):
        context = []

        try:
            for score in text.split(','):
                name, value = score.split(':', 2)
                value = float(value)

                context.append({
                    'id': name,
                    'score': value
                })
        except ValueError:
            raise CLIArgsException('invalid context weights map: ' + text)

        return context

    @staticmethod
    def __context_tostring(context):
        return ', '.join([(score['id'] + ' = ' + ('%.6f' % score['score'])) for score in context])

    def _translate(self, line, _=None):
        return self.api.translate(line, session=self.session, processing=self.processing, nbest=self.nbest)

    def _encode_translation(self, translation):
        return translation['translation'].encode('utf-8')

    def _encode_nbest(self, nbest):
        scores = []

        for name, values in nbest['scores'].iteritems():
            scores.append(name + '= ' + ' '.join([str(f) for f in values]))

        return [
            nbest['translation'],
            ' '.join(scores),
            str(nbest['totalScore'])
        ]

    def execute(self, line):
        pass

    def flush(self):
        pass

    def close(self):
        if self._nbest_file is not None:
            self._nbest_file.close()

        try:
            if self.session is not None:
                self.api.close_session(self.session)
        except:
            # Nothing to do
            pass


class BatchExecutor(Executor):
    def __init__(self, api, args, threads=None):
        Executor.__init__(self, api, args)
        self._pool = multithread.Pool(threads if threads is not None else 100)
        self.jobs = []

    def execute(self, line):
        result = self._pool.apply_async(self._translate, (line, None))
        self.jobs.append(result)

    def flush(self):
        line_id = 0

        for job in self.jobs:
            translation = job.get()

            print self._encode_translation(translation)

            if self.nbest is not None:
                for nbest in translation['nbest']:
                    parts = [str(line_id)] + self._encode_nbest(nbest)
                    self.nbest_out.write((u' ||| '.join(parts)).encode('utf-8'))
                    self.nbest_out.write('\n')

            line_id += 1

    def close(self):
        Executor.close(self)
        self._pool.terminate()


class InteractiveExecutor(Executor):
    def __init__(self, api, args):
        Executor.__init__(self, api, args)

    def execute(self, line):
        try:
            translation = self._translate(line)

            if self.nbest is not None:
                for nbest in translation['nbest']:
                    self.nbest_out.write((u' ||| '.join(self._encode_nbest(nbest))).encode('utf-8'))
                    self.nbest_out.write('\n')

            print self._encode_translation(translation)
        except requests.exceptions.ConnectionError:
            print 'CONNECTION ERROR: unable to connect MMT server, try starting it with "./mmt start"'
        except requests.exceptions.HTTPError as e:
            print 'HTTP ERROR: ' + e.message


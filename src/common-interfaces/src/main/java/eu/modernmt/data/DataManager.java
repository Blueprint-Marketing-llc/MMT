package eu.modernmt.data;

import eu.modernmt.model.ImportJob;
import eu.modernmt.model.corpus.BilingualCorpus;

import java.io.Closeable;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Created by davide on 06/09/16.
 */
public interface DataManager extends Closeable {

    short DOMAIN_UPLOAD_CHANNEL_ID = 0;
    short CONTRIBUTIONS_CHANNEL_ID = 1;

    interface Listener {

        void onDataBatchProcessed(Map<Short, Long> updatedPositions);

    }

    @Deprecated
    Map<Short, Long> connect(long timeout, TimeUnit unit) throws HostUnreachableException;

    Map<Short, Long> connect(String host, int port, long timeout, TimeUnit unit) throws HostUnreachableException;

    void setDataManagerListener(Listener listener);

    void addDataListener(DataListener listener);

    ImportJob upload(int domainId, BilingualCorpus corpus, short channel) throws DataManagerException;

    ImportJob upload(int domainId, BilingualCorpus corpus, DataChannel channel) throws DataManagerException;

    void upload(int domainId, String sourceSentence, String targetSentence, short channel) throws DataManagerException;

    void upload(int domainId, String sourceSentence, String targetSentence, DataChannel channel) throws DataManagerException;

    DataChannel getDataChannel(short id);

    Map<Short, Long> getChannelsPositions();

    void waitChannelPosition(short channel, long position) throws InterruptedException;

    void waitChannelPositions(Map<Short, Long> positions) throws InterruptedException;

}

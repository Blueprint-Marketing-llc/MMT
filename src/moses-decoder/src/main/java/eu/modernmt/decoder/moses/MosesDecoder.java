package eu.modernmt.decoder.moses;

import eu.modernmt.aligner.Aligner;
import eu.modernmt.context.ContextScore;
import eu.modernmt.decoder.Decoder;
import eu.modernmt.decoder.DecoderFeature;
import eu.modernmt.decoder.DecoderTranslation;
import eu.modernmt.decoder.TranslationSession;
import eu.modernmt.model.Sentence;
import eu.modernmt.model.Word;
import eu.modernmt.updating.Update;
import eu.modernmt.updating.UpdatesListener;
import eu.modernmt.vocabulary.Vocabulary;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Created by davide on 26/11/15.
 */
public class MosesDecoder implements Decoder, UpdatesListener {

    private static final Logger logger = LogManager.getLogger(MosesDecoder.class);

    static {
        try {
            System.loadLibrary("jnimoses");
            logger.info("Library jnimoses loaded successfully");
        } catch (Throwable e) {
            logger.error("Unable to load library jnimoses", e);
            throw e;
        }
    }

    private final HashMap<Long, Long> sessions = new HashMap<>();
    private long nativeHandle;

    public MosesDecoder(File iniFile, Aligner aligner, Vocabulary vocabulary) throws IOException {
        if (!iniFile.isFile())
            throw new IOException("Invalid INI file: " + iniFile);

        this.nativeHandle = instantiate(iniFile.getAbsolutePath(),
                aligner.getNativeHandle(), vocabulary.getNativeHandle());
    }

    private native long instantiate(String inifile, long aligner, long vocabulary);

    // Features

    @Override
    public native MosesFeature[] getFeatures();

    @Override
    public float[] getFeatureWeights(DecoderFeature feature) {
        return getFeatureWeightsFromPointer(((MosesFeature) feature).getNativePointer());
    }

    private native float[] getFeatureWeightsFromPointer(long ptr);

    @Override
    public void setDefaultFeatureWeights(Map<DecoderFeature, float[]> map) {
        Set<DecoderFeature> keys = map.keySet();
        String[] features = new String[keys.size()];
        float[][] weights = new float[keys.size()][];

        int i = 0;
        for (DecoderFeature feature : keys) {
            features[i] = feature.getName();
            weights[i] = map.get(feature);

            i++;
        }

        this.setFeatureWeights(features, weights);
    }

    private native void setFeatureWeights(String[] features, float[][] weights);

    // Translation session

    private long getOrComputeSession(final TranslationSession session) {
        return sessions.computeIfAbsent(session.getId(), key -> {
            ContextXObject context = ContextXObject.build(session.getTranslationContext());
            return createSession(context.keys, context.values);
        });
    }

    private native long createSession(int[] contextKeys, float[] contextValues);

    @Override
    public void closeSession(TranslationSession session) {
        Long internalId = this.sessions.remove(session.getId());
        if (internalId != null) {
            this.destroySession(internalId);

            if (logger.isDebugEnabled())
                logger.debug(String.format("Session %d(%d) destroyed.", session.getId(), internalId));
        }
    }

    private native void destroySession(long internalId);

    // Translate

    @Override
    public DecoderTranslation translate(Sentence text) {
        return translate(text, null, null, 0);
    }

    @Override
    public DecoderTranslation translate(Sentence text, List<ContextScore> translationContext) {
        return translate(text, translationContext, null, 0);
    }

    @Override
    public DecoderTranslation translate(Sentence text, TranslationSession session) {
        return translate(text, null, session, 0);
    }

    @Override
    public DecoderTranslation translate(Sentence text, int nbestListSize) {
        return translate(text, null, null, nbestListSize);
    }

    @Override
    public DecoderTranslation translate(Sentence text, List<ContextScore> translationContext, int nbestListSize) {
        return translate(text, translationContext, null, nbestListSize);
    }

    @Override
    public DecoderTranslation translate(Sentence text, TranslationSession session, int nbestListSize) {
        return translate(text, null, session, nbestListSize);
    }

    private DecoderTranslation translate(Sentence sentence, List<ContextScore> translationContext, TranslationSession session, int nbest) {
        Word[] sourceWords = sentence.getWords();
        if (sourceWords.length == 0)
            return new DecoderTranslation(new Word[0], sentence, null);

        String text = XUtils.join(sourceWords);

        long sessionId = session == null ? 0L : getOrComputeSession(session);
        ContextXObject context = ContextXObject.build(translationContext);

        if (logger.isDebugEnabled()) {
            logger.debug("Translating: \"" + text + "\"");
        }

        long start = System.currentTimeMillis();
        TranslationXObject xtranslation = this.translate(text, context == null ? null : context.keys, context == null ? null : context.values, sessionId, nbest);
        long elapsed = System.currentTimeMillis() - start;

        DecoderTranslation translation = xtranslation.getTranslation(sentence);
        translation.setElapsedTime(elapsed);

        logger.info("Translation of " + sentence.length() + " words took " + (((double) elapsed) / 1000.) + "s");

        return translation;
    }

    private native TranslationXObject translate(String text, int[] contextKeys, float[] contextValues, long session, int nbest);

    // Updates

    @Override
    public void updateReceived(Update update) throws Exception {
        int[] sourceSentence = XUtils.encode(update.sourceSentence.getWords());
        int[] targetSentence = XUtils.encode(update.targetSentence.getWords());
        int[] alignment = XUtils.encode(update.alignment);

        updateReceived(update.streamId, update.sentenceId, update.domain, sourceSentence, targetSentence, alignment);
    }

    private native void updateReceived(int streamId, long sentenceId, int domainId, int[] sourceSentence, int[] targetSentence, int[] alignment);

    @Override
    public Map<Integer, Long> getLatestSequentialNumbers() {
        long[] ids = getLatestUpdatesIdentifier();

        HashMap<Integer, Long> map = new HashMap<>(ids.length);
        for (int i = 0; i < ids.length; i++) {
            if (ids[i] >= 0)
                map.put(i, ids[i]);
        }

        return map;
    }

    private native long[] getLatestUpdatesIdentifier();

    // Shutdown

    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        nativeHandle = dispose(nativeHandle);
    }

    @Override
    public void close() {
        sessions.values().forEach(this::destroySession);
        nativeHandle = dispose(nativeHandle);
    }

    private native long dispose(long handle);

}

// Copyright Verizon Media. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package ai.vespa.reindexing;

import ai.vespa.reindexing.Reindexer.Cluster;
import ai.vespa.reindexing.Reindexing.Status;
import ai.vespa.reindexing.ReindexingCurator.ReindexingLockException;
import com.yahoo.document.Document;
import com.yahoo.document.DocumentPut;
import com.yahoo.document.DocumentType;
import com.yahoo.document.DocumentTypeManager;
import com.yahoo.document.config.DocumentmanagerConfig;
import com.yahoo.documentapi.DocumentAccessParams;
import com.yahoo.documentapi.ProgressToken;
import com.yahoo.documentapi.SyncParameters;
import com.yahoo.documentapi.VisitorParameters;
import com.yahoo.documentapi.local.LocalDocumentAccess;
import com.yahoo.documentapi.messagebus.protocol.DocumentProtocol;
import com.yahoo.searchdefinition.derived.Deriver;
import com.yahoo.test.ManualClock;
import com.yahoo.vespa.curator.mock.MockCurator;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Timeout;

import java.time.Duration;
import java.time.Instant;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.Phaser;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertSame;
import static org.junit.jupiter.api.Assertions.assertThrows;

/**
 * @author jonmv
 */
class ReindexerTest {

    final DocumentmanagerConfig musicConfig = Deriver.getDocumentManagerConfig("src/test/resources/schemas/music.sd").build();
    final DocumentTypeManager manager = new DocumentTypeManager(musicConfig);
    final DocumentType music = manager.getDocumentType("music");
    final Document document = new Document(music, "id:ns:music::one");
    final Cluster cluster = new Cluster("cluster", "id", Map.of(music, "default"));
    final ManualClock clock = new ManualClock(Instant.EPOCH);

    ReindexingCurator database;
    LocalDocumentAccess access;

    @BeforeEach
    void setUp() {
        database = new ReindexingCurator(new MockCurator(), manager, Duration.ofMillis(1));
        access = new LocalDocumentAccess(new DocumentAccessParams().setDocumentmanagerConfig(musicConfig));
    }

    @Test
    void throwsWhenUnknownBuckets() {
        assertThrows(NullPointerException.class,
                     () -> new Reindexer(new Cluster("cluster", "id", Map.of()),
                                         Map.of(music, Instant.EPOCH),
                                         database,
                                         access,
                                         clock));
    }

    @Test
    void throwsWhenLockHeldElsewhere() throws InterruptedException, ExecutionException {
        Reindexer reindexer = new Reindexer(cluster, Map.of(music, Instant.EPOCH), database, access, clock);
        Executors.newSingleThreadExecutor().submit(database::lockReindexing).get();
        assertThrows(ReindexingLockException.class, reindexer::reindex);
    }

    @Test
    @Timeout(10)
    void nothingToDo() throws ReindexingLockException {
        Reindexer reindexer = new Reindexer(cluster, Map.of(), database, access, clock);
        access.createSyncSession(new SyncParameters.Builder().build()).put(new DocumentPut(document));
        access.setPhaser(new Phaser(1)); // Would block any visiting until timeout.
        reindexer.reindex();
    }

    @Test
    void parameters() throws ReindexingLockException {
        Reindexer reindexer = new Reindexer(cluster, Map.of(), database, access, clock);
        ProgressToken token = new ProgressToken();
        VisitorParameters parameters = reindexer.createParameters(music, token);
        assertEquals("music:artist", parameters.getFieldSet());
        assertSame(token, parameters.getResumeToken());
        assertEquals("default", parameters.getBucketSpace());
        assertEquals("[Storage:cluster=cluster;clusterconfigid=id]", parameters.getRoute().toString());
        assertEquals("cluster", parameters.getRemoteDataHandler());
        assertEquals("music", parameters.getDocumentSelection());
        assertEquals(DocumentProtocol.Priority.LOW_1, parameters.getPriority());
    }

    @Test
    @Timeout(10)
    void testReindexing() throws ReindexingLockException, ExecutionException, InterruptedException {
        // Reindexer is told to update "music" documents no earlier than EPOCH, which is just now.
        Reindexer reindexer = new Reindexer(cluster, Map.of(music, Instant.EPOCH), database, access, clock);
        Phaser phaser = new Phaser(1);
        access.setPhaser(phaser); // Will block any visiting until timeout.
        reindexer.reindex();

        // Since "music" was a new document type, it is stored as just reindexed, and nothing else happens.
        Reindexing reindexing = Reindexing.empty().with(music, Status.ready(Instant.EPOCH).running().successful(Instant.EPOCH));
        assertEquals(reindexing, database.readReindexing());

        // New config tells reindexer to reindex "music" documents no earlier than at 10 millis after EPOCH, which isn't yet.
        clock.advance(Duration.ofMillis(5));
        reindexer = new Reindexer(cluster, Map.of(music, Instant.ofEpochMilli(10)), database, access, clock);
        reindexer.reindex(); // Nothing happens because it's not yet time. This isn't supposed to happen unless high clock skew.
        assertEquals(reindexing, database.readReindexing());

        // It's time to reindex the "music" documents, but since we sneak a FAILED status in there, nothing is done.
        clock.advance(Duration.ofMillis(10));
        Reindexing failed = Reindexing.empty().with(music, Status.ready(clock.instant()).running().failed(clock.instant(), "fail"));
        database.writeReindexing(failed);
        reindexer = new Reindexer(cluster, Map.of(music, Instant.ofEpochMilli(10)), database, access, clock);
        reindexer.reindex(); // Nothing happens because status is already FAILED for the document type.
        assertEquals(failed, database.readReindexing());

        // It's time to reindex the "music" documents — none yet, so this is a no-op.
        database.writeReindexing(reindexing); // Restore state where reindexing was complete at 5 ms after EPOCH.
        ExecutorService executor = Executors.newSingleThreadExecutor();
        Future<?> future = executor.submit(uncheckedReindex(reindexer));
        while (phaser.getRegisteredParties() == 1)
            Thread.sleep(1); // Need to wait for the visitor to register, without any proper way of doing it >_<
        phaser.arriveAndAwaitAdvance(); // Visitor has arrived — so should we.
        future.get(); // Write state to database.
        reindexing = reindexing.with(music, Status.ready(clock.instant()).running().successful(clock.instant()));
        assertEquals(reindexing, database.readReindexing());

        // We add a document and interrupt visiting before this document is visited.
        access.createSyncSession(new SyncParameters.Builder().build()).put(new DocumentPut(document));
        clock.advance(Duration.ofMillis(10));
        reindexer = new Reindexer(cluster, Map.of(music, Instant.ofEpochMilli(20)), database, access, clock);
        future = executor.submit(uncheckedReindex(reindexer));
        while (phaser.getRegisteredParties() == 1)
            Thread.sleep(1); // Need to wait for the visitor to register, without any proper way of doing it >_<
        database.writeReindexing(Reindexing.empty()); // Wreck database while running, to verify we write the expected value.
        phaser.arriveAndAwaitAdvance(); // Synchronize with visitor, which may now send the document.
        executor.shutdownNow(); // Interrupt the visit before it completes.
        future.get(); // Write state to database.
        reindexing = reindexing.with(music, Status.ready(clock.instant()).running().halted());
        assertEquals(reindexing, database.readReindexing());

        // Manually put a progress token in there, to verify this is used when resuming, and that we store what we retrieve.
        reindexing = reindexing.with(music, Status.ready(clock.instant()).running().progressed(new ProgressToken()).halted());
        database.writeReindexing(reindexing);
        phaser = new Phaser(1);
        access.setPhaser(phaser);
        executor = Executors.newSingleThreadExecutor();
        future = executor.submit(uncheckedReindex(reindexer));
        while (phaser.getRegisteredParties() == 1)
            Thread.sleep(1); // Need to wait for the visitor to register, without any proper way of doing it >_<
        database.writeReindexing(Reindexing.empty()); // Wreck database while running, to verify we write the expected value.
        phaser.arriveAndAwaitAdvance(); // Synchronize with visitor, which may now send the document.
        executor.shutdownNow(); // Interrupt the visit before it completes.
        future.get(); // Write state to database.
        assertEquals(reindexing, database.readReindexing());

        // Finally let the visit complete.
        phaser = new Phaser(1);
        access.setPhaser(phaser);
        executor = Executors.newSingleThreadExecutor();
        future = executor.submit(uncheckedReindex(reindexer));
        while (phaser.getRegisteredParties() == 1)
            Thread.sleep(1); // Need to wait for the visitor to register, without any proper way of doing it >_<
        database.writeReindexing(Reindexing.empty()); // Wreck database while running, to verify we write the expected value.
        phaser.arriveAndAwaitAdvance(); // Synchronize with visitor, which may now send the document.
        phaser.arriveAndAwaitAdvance(); // Synchronize with visitor, which may now complete.
        phaser.arriveAndAwaitAdvance(); // Synchronize with visitor, which has now completed.
        future.get(); // Write state to database.
        reindexing = reindexing.with(music, Status.ready(clock.instant()).running().successful(clock.instant()));
        assertEquals(reindexing, database.readReindexing());
    }

    Callable<Void> uncheckedReindex(Reindexer reindexer) {
        return () -> { reindexer.reindex(); return null; };
    }

}

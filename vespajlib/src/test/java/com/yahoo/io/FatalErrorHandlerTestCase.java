// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.io;

import static org.junit.Assert.*;

import java.security.Permission;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

/**
 * Just to remove noise from the coverage report.
 *
 * @author <a href="mailto:steinar@yahoo-inc.com">Steinar Knutsen</a>
 */
public class FatalErrorHandlerTestCase {
    private static final class AvoidExiting extends SecurityManager {

        @Override
        public void checkPermission(Permission perm) {
        }

        @Override
        public void checkExit(int status) {
            throw new SecurityException();
        }

    }

    private FatalErrorHandler h;

    @Before
    public void setUp() throws Exception {
        h = new FatalErrorHandler();
        System.setSecurityManager(new AvoidExiting());
    }

    @After
    public void tearDown() throws Exception {
        System.setSecurityManager(null);
    }

    @Test
    public final void testHandle() {
        boolean caught = false;
        try {
            h.handle(new Throwable(), "abc");
        } catch (SecurityException e) {
            caught = true;
        }
        assertTrue(caught);
    }

}

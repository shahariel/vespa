// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.jdisc.application;

import org.junit.Test;
import org.mockito.Mockito;
import org.osgi.framework.Bundle;

import java.util.Arrays;
import java.util.Collection;
import java.util.LinkedList;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.fail;


/**
 * @author Simon Thoresen Hult
 */
public class BundleInstallationExceptionTestCase {

    @Test
    public void requireThatAccessorsWork() {
        Throwable t = new Throwable("foo");
        Collection<Bundle> bundles = new LinkedList<>();
        bundles.add(Mockito.mock(Bundle.class));
        BundleInstallationException e = new BundleInstallationException(bundles, t);
        assertSame(t, e.getCause());
        assertEquals(t.getMessage(), e.getCause().getMessage());
        assertEquals(bundles, e.installedBundles());
    }

    @Test
    public void requireThatBundlesCollectionIsDefensivelyCopied() {
        Collection<Bundle> bundles = new LinkedList<>();
        bundles.add(Mockito.mock(Bundle.class));
        BundleInstallationException e = new BundleInstallationException(bundles, new Throwable());
        bundles.add(Mockito.mock(Bundle.class));
        assertEquals(1, e.installedBundles().size());
    }

    @Test
    public void requireThatBundlesCollectionIsUnmodifiable() {
        BundleInstallationException e = new BundleInstallationException(Arrays.asList(Mockito.mock(Bundle.class)),
                                                                        new Throwable());
        try {
            e.installedBundles().add(Mockito.mock(Bundle.class));
            fail();
        } catch (UnsupportedOperationException f) {

        }
    }
}

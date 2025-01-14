// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.slime;

final class NixValue extends Value {
    private static final NixValue invalidNix = new NixValue();
    private static final NixValue validNix = new NixValue();
    private NixValue() {}
    public final Type type() { return Type.NIX; }
    public final void accept(Visitor v) {
        if (valid()) {
            v.visitNix();
        } else {
            v.visitInvalid();
        }
    }
    public static NixValue invalid() { return invalidNix; }
    public static NixValue instance() { return validNix; }
}

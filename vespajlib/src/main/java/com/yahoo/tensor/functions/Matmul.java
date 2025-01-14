// Copyright Yahoo. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.tensor.functions;

import com.google.common.collect.ImmutableList;
import com.yahoo.tensor.TensorType;
import com.yahoo.tensor.evaluation.Name;

import java.util.List;

/**
 * @author bratseth
 */
public class Matmul<NAMETYPE extends Name> extends CompositeTensorFunction<NAMETYPE> {

    private final TensorFunction<NAMETYPE> argument1, argument2;
    private final String dimension;

    public Matmul(TensorFunction<NAMETYPE> argument1, TensorFunction<NAMETYPE> argument2, String dimension) {
        this.argument1 = argument1;
        this.argument2 = argument2;
        this.dimension = dimension;
    }

    public static TensorType outputType(TensorType a, TensorType b, String dimension) {
        return Join.outputType(a, b);
    }

    @Override
    public List<TensorFunction<NAMETYPE>> arguments() { return ImmutableList.of(argument1, argument2); }

    @Override
    public TensorFunction<NAMETYPE> withArguments(List<TensorFunction<NAMETYPE>> arguments) {
        if ( arguments.size() != 2)
            throw new IllegalArgumentException("Matmul must have 2 arguments, got " + arguments.size());
        return new Matmul<>(arguments.get(0), arguments.get(1), dimension);
    }

    @Override
    public PrimitiveTensorFunction<NAMETYPE> toPrimitive() {
        TensorFunction<NAMETYPE> primitiveArgument1 = argument1.toPrimitive();
        TensorFunction<NAMETYPE> primitiveArgument2 = argument2.toPrimitive();
        return new Reduce<>(new Join<>(primitiveArgument1, primitiveArgument2, ScalarFunctions.multiply()),
                            Reduce.Aggregator.sum,
                            dimension);
    }

    @Override
    public String toString(ToStringContext context) {
        return "matmul(" + argument1.toString(context) + ", " + argument2.toString(context) + ", " + dimension + ")";
    }

}

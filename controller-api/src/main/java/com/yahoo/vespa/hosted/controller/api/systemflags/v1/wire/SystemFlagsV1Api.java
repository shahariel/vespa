// Copyright 2019 Oath Inc. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.controller.api.systemflags.v1.wire;

import javax.ws.rs.Consumes;
import javax.ws.rs.DefaultValue;
import javax.ws.rs.PUT;
import javax.ws.rs.Path;
import javax.ws.rs.Produces;
import javax.ws.rs.QueryParam;
import javax.ws.rs.core.MediaType;
import java.io.InputStream;

/**
 * @author bjorncs
 */
@Path("/system-flags/v1")
public interface SystemFlagsV1Api {

    @PUT
    @Produces(MediaType.APPLICATION_JSON)
    @Consumes("application/zip")
    @Path("/deploy")
    WireSystemFlagsDeployResult deploy(@QueryParam("dryRun") @DefaultValue("false") boolean dryRun, InputStream inputStream);

}

/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    The source code contained or described herein and all documents related
    to the source code ("Material") are owned by Intel Corporation or its
    suppliers or licensors.  Title to the Material remains with Intel
    Corporation or its suppliers and licensors.  The Material is protected
    by worldwide copyright laws and treaty provisions.  No part of the
    Material may be used, copied, reproduced, modified, published, uploaded,
    posted, transmitted, distributed, or disclosed in any way without
    Intel's prior express written permission.

    No license under any patent, copyright, trade secret or other
    intellectual property right is granted to or conferred upon you by
    disclosure or delivery of the Materials, either expressly, by
    implication, inducement, estoppel or otherwise.  Any license under such
    intellectual property rights must be express and approved by Intel in
    writing.
*/

#if __APPLE__

#define HARNESS_CUSTOM_MAIN 1
#include "harness.h"
#include <cstdlib>
#include "tbb/task_scheduler_init.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

bool exec_test(const char *self) {
    int status = 1;
    pid_t p = fork();
    if(p < 0) {
        REPORT("fork error: errno=%d: %s\n", errno, strerror(errno));
        return true;
    }
    else if(p) { // parent
        if(waitpid(p, &status, 0) != p) {
            REPORT("wait error: errno=%d: %s\n", errno, strerror(errno));
            return true;
        }
        if(WIFEXITED(status)) {
            if(!WEXITSTATUS(status)) return false; // ok
            else REPORT("child has exited with return code 0x%x\n", WEXITSTATUS(status));
        } else {
            REPORT("child error 0x%x:%s%s ", status, WIFSIGNALED(status)?" signalled":"",
                WIFSTOPPED(status)?" stopped":"");
            if(WIFSIGNALED(status))
                REPORT("%s%s", sys_siglist[WTERMSIG(status)], WCOREDUMP(status)?" core dumped":"");
            if(WIFSTOPPED(status))
                REPORT("with %d stop-code", WSTOPSIG(status));
            REPORT("\n");
        }
    }
    else { // child
        // reproduces error much often
        execl(self, self, "0", NULL);
        REPORT("exec fails %s: %d: %s\n", self, errno, strerror(errno));
        exit(2);
    }
    return true;
}

HARNESS_EXPORT
int main( int argc, char * argv[] ) {
    MinThread = 3000;
    ParseCommandLine( argc, argv );
    if( MinThread <= 0 ) {
        tbb::task_scheduler_init init( 2 ); // even number required for an error
    } else {
        for(int i = 0; i<MinThread; i++) {
            if(exec_test(argv[0])) {
                REPORT("ERROR: execution fails at %d-th iteration!\n", i);
                exit(1);
            }
        }
        REPORT("done\n");
    }
}

#else /* !__APPLE__ */

#define HARNESS_NO_PARSE_COMMAND_LINE 1
#include "harness.h"

int TestMain () {
    return Harness::Skipped;
}

#endif /* !__APPLE__ */

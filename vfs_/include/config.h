#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "log.h"
#include "size.h"

// ~~~~~~~~~~~ Config ~~~~~~~~~~

/* TODO (for future builds):
    - Add functionality to prompt user with their own system, rather a segment of a singleton when using rfs. (Will allow user to only see their own disks created under their own account).
    - This will avoid data racing as a whole, allow disks to be shared with more than one user. Each remotely logged in user, will see their own version of the mp_vfs.
    - Segment fread into buffer to reduce ram usage. (As well as other parts that store large amount of bytes into local/remote memory).
    - Ensure project compiles on windows with new features added.
    - Pass user obj from server.cpp to terminal.cpp to login into, from there a user can see his disks once logged in. When remote or local, to view disks owned by the user. 
      Each user, can open multiple mnted systems, if more than one session is being used per user account.

        fat32
*/

#define _DEBUG_ 0

#define CFG_USER_SPACE_SIZE       (uint64_t)(MB(480))
#define CFG_CLUSTER_SIZE          (uint64_t)(KB(12))
#define CFG_MAX_USER_SPACE_SIZE   (uint64_t)(GB(4))
#define CFG_MIN_USER_SPACE_SIZE   (uint64_t)(MB(24))

#define CFG_SOCK_OPEN             (int8_t)1
#define CFG_SOCK_CLOSE            (int8_t)0
#define CFG_DEFAULT_PORT          (uint32_t)52222
#define CFG_SOCK_LISTEN_AMT       (uint32_t)5

#define CFG_PACKET_SIGNATURE      (char*)"[_VFS_]\0"
#define CFG_PACKET_SIGNATURE_SIZE (strlen(CFG_PACKET_SIGNATURE))

#define CFG_FLAGS_BUFFER_SIZE     (size_t)100
#define CFG_PAYLOAD_SIZE          (KB(1))
#define CFG_PACKET_HASH_SIZE      (size_t)10

#endif // _CONFIG_H

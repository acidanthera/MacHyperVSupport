//
//  hvsnapshotd.c
//  Hyper-V VSS snapshot userspace daemon
//
//  Copyright © 2025 Goldfish64. All rights reserved.
//

#include "hvdebug.h"
#include "hviokit.h"

HVDeclareLogFunctionsUser("hvsnapshotd");

void hvIOKitNotificationHandler(io_connect_t connection, CFMachPortRef port, void *msg, CFIndex size, void *info) {
  
}

int main(int argc, const char * argv[]) {
  HVSYSLOG(stdout, "started");
  
  // Get /.
  
  int fd = open("/System/Volumes/Data", O_RDONLY);
  
  HVSYSLOG(stdout, "Open /System/Volumes/Data 0x%X", fd);
  
  int ret = fcntl(fd, F_FREEZE_FS, 0);
  
  HVSYSLOG(stdout, "Is frozen: 0x%X", ret);
  
  sleep(10);
  
  ret = fcntl(fd, F_THAW_FS, 0);
  
  HVSYSLOG(stdout, "Is thawed: 0x%X", ret);

  //
  // Run main loop, this should not return.
  //
  //CFRunLoopRun();
  return 0;
}

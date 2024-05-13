# OSC 2024
|Github Account|Student ID|Name|
|-|-|-|
|hankliou|B122510|Han-Yang Liou|

## Lab 2
- remind to copy initramfs.cpio and .dtb file to sdcard.

## lab 3
- test async uart by 'testAsyncUart' command.
- setTimer args format is 'setTimer [msg] [time(second)]', do not add any extra white space in command, or it may not get the command corectly.

## lab 5
/**
                                                                   (                                                                          )
[Signal]  [UserProcess]                                            (                    [ Registered Signal Handler ]                         )     [UserProcess]
   |             \                                                 (                   /                             \                        )    /
   |              \                                                (                  /                               \                       )   /
---|---------------[SystemCall]------------------------------------(-----------------/---------------------------------\----------------------)--/-------------------------
   |                           \                 [Signal Check]    (-> <is Registered>                                  \                     ) /
   V                            \                   /      ^       (                 \                                   \                    )/
 [Job Pending]                   [Exception Handler]       |       (                  \ ---[ Default Signal Handler ]---- [Exception Handler])
   |                                                       |       (                                                                          )
   -------<trigger only if UserProcess do syscall>----------        (               CONTENT SWITCHING FOR SIGNAL HANDLER                       )

**/
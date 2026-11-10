/* Whether macOS lets this process record from the microphone. */
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include <stdio.h>

int MacMicrophone_Allowed(void)
{
    static int reported;
    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if (status == AVAuthorizationStatusAuthorized)
        return 1;
    if (status == AVAuthorizationStatusNotDetermined) {
        /* macOS ends a program that asks without saying why it wants the
         * microphone, so ask only when the bundle carries that text. */
        if (![[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSMicrophoneUsageDescription"]) {
            if (!reported) {
                fputs("Voice chat: run the game from its app, or from a program that already has microphone access, to record.\n", stderr);
                reported = 1;
            }
            return 0;
        }
        static int asked;
        if (!asked) {
            asked = 1;
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) { (void)granted; }];
        }
        return 0;
    }
    if (!reported) {
        fputs("Voice chat: microphone access is off for this app. Turn it on in System Settings, Privacy & Security, Microphone.\n", stderr);
        reported = 1;
    }
    return 0;
}

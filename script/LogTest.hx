package;

import me.internal.LogInterface;
import me.Log;

class LogTest {
    public static function DoStuff(): Void {
        LogInterface.log_info("");
        Log.Info("check it out, this is only written in haxe!!!!");
    }
}

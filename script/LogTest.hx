package;

import me.bindings.LogBindings;
import me.Log;

class LogTest {
    public static function DoStuff(): Void {
        LogBindings.log_info("");
        Log.Info("check it out, this is only written in haxe!!!!");
    }
}

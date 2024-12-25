package;

import me.Time;
import me.Log;
import me.Update;

@:keep
class TestUpdate extends Update {
    var logged: Bool = false;

    override function new() {
        super();
        Log.Info("Test update created");
    }

    override function Update(): Void {
        var result = Math.floor(Time.ElapsedAsDouble);
        if (result % 2 == 0) {
            if (!logged) Log.Info('It has been ${result} seconds.');
        } else {
            logged = false;
        }
    }
}

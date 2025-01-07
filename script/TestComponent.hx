package;

import me.Time;
import me.math.Vector3;
import me.game.Component;

class TestComponent extends Component {
    static var radius = 2;

    override function OnUpdate() {
        var vector = Vector3.Zero();
        vector.x = Math.sin(Time.Elapsed) * radius;
        vector.z = Math.cos(Time.Elapsed) * radius;
        Transform.Position = vector;
    }
}

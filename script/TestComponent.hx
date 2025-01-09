package;

import me.asset.MeshAsset;
import me.components.MeshRenderer;
import me.Time;
import me.math.Vector3;
import me.game.Component;

class TestComponent extends Component {
    static var radius = 2;
    static var mesh: MeshAsset = null;

    override function OnStart() {
        var renderer = Components.Create(MeshRenderer);
        renderer.Mesh = mesh;
    }

    override function OnUpdate() {
        var vector = Vector3.Zero();
        vector.x = Math.sin(Time.Elapsed) * radius;
        vector.z = Math.cos(Time.Elapsed) * radius;
        Transform.Position = vector;
    }
}

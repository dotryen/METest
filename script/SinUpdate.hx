package;

import me.SceneSystem;
import me.Log;
import me.math.Vector3;
import me.Time;
import me.math.Transform;
import me.scene.SceneTransform;
import me.Update;

final radius: Single = 5;

class SinUpdate extends Update {
    var target: SceneTransform;
    
    var transformCache: Transform;
    var vecCache: Vector3;

    override function Update(): Void {
        if (transformCache == null) {
            transformCache = Transform.Identity();
            vecCache = Vector3.Zero();
        }

        // var example = radius + radius;
        var time = Time.Elapsed;
        vecCache.x = Math.sin(time);
        vecCache.z = Math.cos(time);
        target.Position = vecCache;
    }

    public function PrintCache(): Void {
        Log.Info(vecCache);
    }

    public function PrintPos(): Void {
        Log.Info("Transform's position is " + target.Global.Position);
    }

    public function PrintTime(): Void {
        Log.Info("Current time is " + Time.Elapsed);
    }

    public function DoMath(): Void {
        var example = radius + radius;
    }

    public function GetSceneCount(): Void {
        Log.Info('Scene count: ${SceneSystem.instance.SceneCount}');
    }
}

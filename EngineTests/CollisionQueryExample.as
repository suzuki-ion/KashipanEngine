// Compile-only usage example; invoke QueryExample from a scene script.
void QueryExample() {
    CollisionShape3D sphere;
    sphere.type = CollisionShapeType3D::Sphere;
    sphere.center = Vector3(0, 1, 0);
    sphere.radius = 2;
    array<CollisionHit@>@ hits = QueryCollision3D(sphere, false, GetOwnerObject());
    for (uint i = 0; i < hits.length(); ++i) {
        Log(hits[i].otherObject.GetName());
        Vector3 normal = hits[i].normal;
        float penetration = hits[i].penetration;
    }

    CollisionShape2D box;
    box.type = CollisionShapeType2D::Box;
    box.center = Vector2(2, 3);
    box.halfSize = Vector2(1, 0.5);
    box.rotation = 0.5;
    array<CollisionHit@>@ hits2D = QueryCollision2D(box);

    CollisionShape3D hull;
    hull.type = CollisionShapeType3D::ConvexMesh;
    hull.SetVertices({Vector3(-1,-1,-1), Vector3(1,-1,-1), Vector3(0,1,-1), Vector3(0,0,1)});
    array<CollisionHit@>@ hullHits = QueryCollision3D(hull);
    if (hits.length() > 0) {
        array<CollisionHit@>@ componentHits = QueryCollider(hits[0].otherCollider);
    }
}

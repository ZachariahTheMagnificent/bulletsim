#pragma once
#include <boost/shared_ptr.hpp>
class FakeGripper;
typedef boost::shared_ptr<FakeGripper> FakeGripperPtr;
class FakeObjectCopy;
typedef boost::shared_ptr<FakeObjectCopy> FakeObjectCopyPtr;
class TelekineticGripper;
typedef boost::shared_ptr<TelekineticGripper> TelekineticGripperPtr;
class Finger;
typedef boost::shared_ptr<Finger> FingerPtr;
class Thumb;
typedef boost::shared_ptr<Thumb> ThumbPtr;
class Palm;
typedef boost::shared_ptr<Palm> PalmPtr;
class Hand;
typedef boost::shared_ptr<Hand> HandPtr;
class MyWriteToFile;
typedef boost::shared_ptr<MyWriteToFile> MyWriteToFilePtr;
class TempWriteToFile;
typedef boost::shared_ptr<TempWriteToFile> TempWriteToFilePtr;
class ScreenRecorder;
typedef boost::shared_ptr<ScreenRecorder> ScreenRecorderPtr;
class ScreenCapture;
typedef boost::shared_ptr<ScreenCapture> ScreenCapturePtr;
class ScreenThreadRecorder;
typedef boost::shared_ptr<ScreenThreadRecorder> ScreenThreadRecorderPtr;
class ConsecutiveImageWriter;
typedef boost::shared_ptr<ConsecutiveImageWriter> ConsecutiveImageWriterPtr;
class CapsuleRope;
typedef boost::shared_ptr<CapsuleRope> CapsuleRopePtr;
class PickingMouseHandler;
typedef boost::shared_ptr<PickingMouseHandler> PickingMouseHandlerPtr;
class BulletSoftObject;
typedef boost::shared_ptr<BulletSoftObject> BulletSoftObjectPtr;
class RaveInstance;
typedef boost::shared_ptr<RaveInstance> RaveInstancePtr;
class RaveObject;
typedef boost::shared_ptr<RaveObject> RaveObjectPtr;
class RaveRobotObject;
typedef boost::shared_ptr<RaveRobotObject> RaveRobotObjectPtr;
class RobotManipulator;
typedef boost::shared_ptr<RobotManipulator> RobotManipulatorPtr;
class ScopedRobotSave;
typedef boost::shared_ptr<ScopedRobotSave> ScopedRobotSavePtr;
class PlotObject;
typedef boost::shared_ptr<PlotObject> PlotObjectPtr;
class PlotPoints;
typedef boost::shared_ptr<PlotPoints> PlotPointsPtr;
class PlotLines;
typedef boost::shared_ptr<PlotLines> PlotLinesPtr;
class PlotSpheres;
typedef boost::shared_ptr<PlotSpheres> PlotSpheresPtr;
class PlotBoxes;
typedef boost::shared_ptr<PlotBoxes> PlotBoxesPtr;
class PlotAxes;
typedef boost::shared_ptr<PlotAxes> PlotAxesPtr;
class PlotCurve;
typedef boost::shared_ptr<PlotCurve> PlotCurvePtr;
class ConvexDecomp;
typedef boost::shared_ptr<ConvexDecomp> ConvexDecompPtr;
class EventHandler;
typedef boost::shared_ptr<EventHandler> EventHandlerPtr;
class Scene;
typedef boost::shared_ptr<Scene> ScenePtr;
class BulletObject;
typedef boost::shared_ptr<BulletObject> BulletObjectPtr;
class BulletConstraint;
typedef boost::shared_ptr<BulletConstraint> BulletConstraintPtr;
class GrabberKinematicObject;
typedef boost::shared_ptr<GrabberKinematicObject> GrabberKinematicObjectPtr;
class PlaneStaticObject;
typedef boost::shared_ptr<PlaneStaticObject> PlaneStaticObjectPtr;
class CylinderStaticObject;
typedef boost::shared_ptr<CylinderStaticObject> CylinderStaticObjectPtr;
class SphereObject;
typedef boost::shared_ptr<SphereObject> SphereObjectPtr;
class BoxObject;
typedef boost::shared_ptr<BoxObject> BoxObjectPtr;
class CapsuleObject;
typedef boost::shared_ptr<CapsuleObject> CapsuleObjectPtr;
class CapsuleObjectY;
typedef boost::shared_ptr<CapsuleObjectY> CapsuleObjectYPtr;
class OSGInstance;
typedef boost::shared_ptr<OSGInstance> OSGInstancePtr;
class BulletInstance;
typedef boost::shared_ptr<BulletInstance> BulletInstancePtr;
class EnvironmentObject;
typedef boost::shared_ptr<EnvironmentObject> EnvironmentObjectPtr;
class Environment;
typedef boost::shared_ptr<Environment> EnvironmentPtr;
class Fork;
typedef boost::shared_ptr<Fork> ForkPtr;
class ObjectAction;
typedef boost::shared_ptr<ObjectAction> ObjectActionPtr;

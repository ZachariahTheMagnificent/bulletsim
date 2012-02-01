#include "bullet_io.h"
#include "bullet_typedefs.h"
#include "clouds/comm_pcl.h"
#include "clouds/geom.h"
#include "comm/comm2.h"
#include "config_bullet.h"
#include "config_perception.h"
#include "get_nodes.h"
#include "make_bodies.h"
#include "simplescene.h"
#include "optimization_forces.h"
#include "softbodies.h"
#include "utils_perception.h"
#include "vector_io.h"
#include "visibility.h"
#include "recording.h"

#include <pcl/common/transforms.h>

// WORKING PARAMS FOR SPONGE4:
// test_tetra_track --sigB=0.1 --nIter=10 --friction=100000 --gravity=-0.5 --impulseSize=10

int main(int argc, char* argv[]) {
  //////////// get command line options

  Parser parser;
  TrackingConfig::sigB=.1;
  TrackingConfig::nIter=10;
  BulletConfig::friction = 100000;
  BulletConfig::gravity.setZ(-0.5);
  TrackingConfig::impulseSize=10; 
  SceneConfig::enableRobot = SceneConfig::enableIK = SceneConfig::enableHaptics = false;
  GeneralConfig::scale = 10;
  parser.addGroup(TrackingConfig());
  parser.addGroup(RecordingConfig());
  parser.addGroup(SceneConfig());
  parser.addGroup(GeneralConfig());
  parser.addGroup(BulletConfig());
  parser.read(argc, argv);

  //// comm stuff
  setDataRoot("~/comm/sponge4");
  FileSubscriber pcSub("kinect","pcd");
  CloudMessage cloudMsg;
  FileSubscriber towelSub("sponge_pts","pcd");
  CloudMessage towelPtsMsg; //first message

  ////////////// create scene
  Scene scene;
  static PlotPoints::Ptr kinectPts(new PlotPoints(2));
  CorrPlots corrPlots;
  static PlotPoints::Ptr towelEstPlot(new PlotPoints(4));
  static PlotPoints::Ptr towelObsPlot(new PlotPoints(4));
  towelObsPlot->setDefaultColor(0,1,0,1);

  ////// recording
  ScreenRecorder* rec;
  if (RecordingConfig::record != DONT_RECORD){
    rec = new ScreenRecorder(scene.viewer);
  }


  /////////////// load table
  vector< vector<float> > vv = floatMatFromFile(onceFile("table_corners.txt").string());
  vector<btVector3> tableCornersCam = toBulletVectors(vv);
  CoordinateTransformer CT(getCamToWorldFromTable(tableCornersCam));

  vector<btVector3> tableCornersWorld = CT.toWorldFromCamN(tableCornersCam);
  BulletObject::Ptr table = makeTable(tableCornersWorld, .1*METERS);
  table->setColor(0,0,1,.25);

  //////////////// load towel

  towelPtsMsg.fromFiles(towelSub.m_names.getCur());
  vector<btVector3> towelPtsCam = toBulletVectors(towelPtsMsg.m_data);
  vector<btVector3> towelPtsWorld = CT.toWorldFromCamN(towelPtsCam);
  vector<btVector3> towelCorners = toBulletVectors(getCorners(toEigenVectors(towelPtsWorld)));
  BOOST_FOREACH(btVector3& pt, towelCorners) pt += btVector3(.01*METERS,0,0);

// HACK
  vector<btVector3> newCorners(4);
  const float CM = 2.54/100*METERS;
  newCorners[0] = towelCorners[0];
/*  newCorners[1] = newCorners[0] + (towelCorners[1] - towelCorners[0]).normalized() * 5.75 * CM;
  newCorners[2] = newCorners[1] + (towelCorners[2] - towelCorners[1]).normalized() * 2.5 * CM;
  newCorners[3] = newCorners[2] + (towelCorners[3] - towelCorners[2]).normalized() * 5.75 * CM;
  BulletSoftObject::Ptr sponge = makeBoxFromGrid(newCorners, btVector3(0, 0, 7.5*CM), 5, 15, 15, scene.env->bullet->softBodyWorldInfo);
  sponge->softBody->translate(btVector3(0, 0, -7.5*CM));*/
  newCorners[1] = newCorners[0] + (towelCorners[1] - towelCorners[0]).normalized() * 5 * CM;
  newCorners[2] = newCorners[1] + (towelCorners[2] - towelCorners[1]).normalized() * 8 * CM;
  newCorners[3] = newCorners[2] + (towelCorners[3] - towelCorners[2]).normalized() * 5 * CM;
  BulletSoftObject::Ptr sponge = makeBoxFromGrid(newCorners, btVector3(0, 0, 1.5*CM), 10, 15, 15, scene.env->bullet->softBodyWorldInfo);
//  BulletSoftObject::Ptr sponge = makeTetraBox(newCorners, 2.5*CM, scene.env->bullet->softBodyWorldInfo);

  /// add stuff to scene
  scene.env->add(sponge);
  scene.env->add(table);
  scene.env->add(kinectPts);
  //scene.env->add(towelEstPlot);
 // scene.env->add(towelObsPlot);
  //scene.env->add(corrPlots.m_lines);

  scene.startViewer();
  sponge->setColor(1, 0, 1, 1);

  scene.idle(true);
  int skip = 40;
  while (skip-- > 0) { pcSub.skip(); towelSub.skip(); }
  for (int t=0; !scene.viewer.done(); t++) {
    cout << "time step " << t << endl;
    bool success = pcSub.recv(cloudMsg);
    if (!success) break;
    ENSURE(towelSub.recv(towelPtsMsg));

    ColorCloudPtr cloudCam  = cloudMsg.m_data;
    ColorCloudPtr cloudWorld(new ColorCloud());
    pcl::transformPointCloud(*cloudCam, *cloudWorld, CT.worldFromCamEigen);
    kinectPts->setPoints(cloudWorld);
    kinectPts->forceTransparency(0.5);

    vector<btVector3> towelObsPts =  CT.toWorldFromCamN(toBulletVectors(towelPtsMsg.m_data));
    towelObsPlot->setPoints(towelObsPts);

    for (int iter=0; iter < TrackingConfig::nIter; iter++) {
      cout << "iteration " << iter << endl;
//      scene.idle(true);
      vector<float> pVis = calcVisibility(sponge->softBody.get(), scene.env->bullet->dynamicsWorld, CT.worldFromCamUnscaled.getOrigin()*METERS);
      colorByVisibility(sponge->softBody.get(), pVis, towelEstPlot);

      vector<btVector3> towelEstPts = getNodes(sponge);
      SparseArray corr = toSparseArray(calcCorrProb(toEigenMatrix(towelEstPts), toEigenMatrix(towelObsPts), toVectorXf(pVis), TrackingConfig::sigB, TrackingConfig::outlierParam), TrackingConfig::cutoff);
      corrPlots.update(towelEstPts, towelObsPts, corr);

      vector<btVector3> impulses = calcImpulsesSimple(towelEstPts, towelObsPts, corr, TrackingConfig::impulseSize);
      for (int i=0; i<impulses.size(); i++)
	sponge->softBody->addForce(impulses[i],i);

      if (RecordingConfig::record == EVERY_ITERATION || 
	  RecordingConfig::record == FINAL_ITERATION && iter==TrackingConfig::nIter-1)
	rec->snapshot();
      scene.step(.01);
    }

  }

  return 0;
}

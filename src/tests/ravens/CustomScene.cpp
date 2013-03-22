#include "CustomScene.h"
#include "CustomKeyHandler.h"

/** Rotates a transform TFM by ANG, around the point along
 * x-axis of the transform at a distance RAD away.*/
btTransform rotateByAngle (btTransform &tfm, const float ang, const float rad) {
	float pi = 3.14159265, r = rad*GeneralConfig::scale;

	btTransform WorldToEndEffectorTransform = tfm;

	btTransform initT;
	initT.setIdentity();
	initT.setOrigin(r*btVector3(-1,0,0));

	OpenRAVE::Transform T = OpenRAVE::geometry::matrixFromAxisAngle(OpenRAVE::Vector(0,0,ang));
	btTransform bT = util::toBtTransform(T);
	bT.setOrigin(r*btVector3(1,0,0));

	return WorldToEndEffectorTransform*bT*initT;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////SUTURING NEEDLE/////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Constructor for suturing needle. Creates needle from file.*/
CustomScene::SuturingNeedle::SuturingNeedle(CustomScene * _scene, float _rope_radius, float _segment_len, int _nLinks) :
													scene(*_scene), s_needle_radius(0.015),
													s_needle_mass(1000), s_end_angle(1.57),  s_grasping_gripper('n'),
													rope_radius(_rope_radius), segment_len(_segment_len), nLinks(_nLinks) {

	static const char sNeedle_MODEL_FILE[] = EXPAND(BULLETSIM_DATA_DIR) "/xml/needle.xml";
	KinBodyPtr needle_body = scene.rave->env->ReadKinBodyURI(sNeedle_MODEL_FILE);
	btTransform table_tfm;
	scene.table->motionState->getWorldTransform(table_tfm);

	table_tfm.setOrigin(table_tfm.getOrigin() + METERS*btVector3(0.05, (float)scene.bcn*scene.bcs + 0.01, scene.table->getHalfExtents().z()/METERS + 0.002) );
	needle_body->SetTransform(util::toRaveTransform(table_tfm, 1.0f/METERS));

	s_needle = RaveObject::Ptr(new RaveObject(scene.rave,needle_body,CONVEX_HULL,false));
	vector<BulletObject::Ptr> children = s_needle->getChildren();
	cout<<"children: "<<children.size()<<endl;

	s_needle->setColor(0.97,0.09,0.266,1.0); // set the needle's color
	scene.addPreStepCallback(boost::bind(&CustomScene::SuturingNeedle::setGraspingTransformCallback, this));

	//------------------------ initialize the rope -------------------------------
	vector<btVector3> ctrlPts;

	btVector3 handlePos = getNeedleHandleTransform().getOrigin();
	vector<btTransform> transforms;
	vector<btScalar> lengths;
	for (int i=0; i< nLinks; i++)
		//ctrlPts.push_back(handlePos + METERS*btVector3(segment_len*i - 0.15,0,2*rope_radius));  // horizontal rope
		//ctrlPts.push_back(handlePos + METERS*btVector3(0, segment_len*(i - nLinks/2.0), 2*rope_radius)); //vertical rope
		ctrlPts.push_back(handlePos + METERS*btVector3(-segment_len*i, 0, 2*rope_radius)); //vertical rope

	ropePtr.reset(new CapsuleRope(ctrlPts,METERS*rope_radius));
	scene.env->add(ropePtr);
	ropePtr->setColor(0,1,0,1);
	ropePtr->children[0]->setColor(1,0,0,1);
	ropePtr->children[ropePtr->children.size()-1]->setColor(0,0,1,1);


	//s_needle->ignoreCollisionWith(ropePtr->children[0]->rigidBody.get());
	btTransform needleEndT =  getNeedleHandleTransform();
	//needleEndT.setOrigin(needleEndT.getOrigin() + METERS*(needleEndT.getBasis()*btVector3(0,-0.005,0)));
	//util::drawAxes(needleEndT, .1*METERS, scene.env);

	needle_rope_grab = new Grab(ropePtr->children[0]->rigidBody.get(), getNeedleHandleTransform().getOrigin(),scene.env->bullet->dynamicsWorld);
	scene.addPreStepCallback(boost::bind(&CustomScene::SuturingNeedle::setConnectedRopeTransformCallback, this));
	//------------------------------------------------------------------------------
}

/** Returns transform of needle's tip.*/
btTransform CustomScene::SuturingNeedle::getNeedleTipTransform () {

	btTransform tfm = s_needle->getIndexTransform(0);
	return rotateByAngle(tfm, -s_end_angle, s_needle_radius);
}


/** Returns transform of needle's handle.*/
btTransform CustomScene::SuturingNeedle::getNeedleHandleTransform () {

	btTransform tfm = s_needle->getIndexTransform(0);
	return rotateByAngle(tfm, s_end_angle, s_needle_radius);

}

/** Returns transform of needle's center.
 *  z points in same direction as indexTransform
 *  x points in direction towards origin of index transform.*/
btTransform CustomScene::SuturingNeedle::getNeedleCenterTransform() {

	btTransform tfm = s_needle->getIndexTransform(0);
	btTransform centerTfm;
	centerTfm.getOrigin() = tfm.getOrigin()+tfm.getBasis().getColumn(0)*s_needle_radius*METERS;
	centerTfm.getBasis() = tfm.getBasis()*btMatrix3x3(-1,0,0,0,-1,0,0,0,1);

	return centerTfm;
}


/** Returns true if the point is close to a point on the needle.
 * Approximates needle to a semi-circle (good approximation).
 */
bool CustomScene::SuturingNeedle::pointCloseToNeedle (btVector3 pt) {

	float xDirThresh = 0.0, zDirThresh = 0.2, distThresh = 0.006;

	btTransform tfm = getNeedleCenterTransform();
	btVector3 center = tfm.getOrigin();
	btVector3 xVec = tfm.getBasis().getColumn(0);
	btVector3 zVec = tfm.getBasis().getColumn(2);

	// Vector from center to point being checked
	btVector3 dVec = pt-center;
	// Vector in the plane of needle being checked for distance.
	btVector3 dVecXY = dVec - dVec.dot(zVec)*zVec;

	return (dVec.normalized().dot(xVec) >= xDirThresh) &&
			(fabs(dVec.normalized().dot(zVec)) <= zDirThresh) &&
			(fabs(dVecXY.length()/METERS - s_needle_radius) < distThresh);
}

void CustomScene::SuturingNeedle::setGraspingTransformCallback() {
	if (s_grasping_gripper == 'n') return;

	btTransform nTfm = s_gripperManip->getTransform()*s_grasp_tfm;
	if (s_needle->children[0]->isKinematic)
		s_needle->getChildren()[0]->motionState->setKinematicPos(nTfm);
	else if (s_needle_mass == 0)
		s_needle->getChildren()[0]->motionState->setWorldTransform(nTfm);
}

void CustomScene::SuturingNeedle::setConnectedRopeTransformCallback() {
	btTransform nTfm = getNeedleHandleTransform ();
	needle_rope_grab->updatePosition(nTfm.getOrigin());
}

void CustomScene::SuturingNeedle::getNeedlePoints (vector<btVector3> & needlePoints, float scale) {

	needlePoints.clear();
	unsigned int num_points = 30;

	btTransform tfm = s_needle->getIndexTransform(0);
	float ang_diff = 2*s_end_angle;

	for (int i = 0; i < num_points; ++i)
		needlePoints.push_back(rotateByAngle (tfm, -s_end_angle + (float) i/(num_points - 1)*ang_diff, s_needle_radius).getOrigin()*scale);
}

void CustomScene::SuturingNeedle::getRopePoints (bool nodes, vector<btVector3> & ropePoints, float scale) {
	if (nodes) ropePoints = ropePtr->getNodes();
	else ropePoints = ropePtr->getControlPoints();

	for (int i = 0; i < ropePoints.size(); ++i) ropePoints[i]  = ropePoints[i]*scale;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////SUTURING PEG///////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

CustomScene::SuturingPeg::SuturingPeg (CustomScene * _scene, RaveRobotObject::Manipulator::Ptr _p_gripperManip,
										float _p_rad, float _p_len,
										float _rope_radius, float _segment_len, int _nLinks):
										scene(*_scene), p_gripperManip(_p_gripperManip),
										p_radius(_p_rad), p_len(_p_len), p_grasping_finger1(false),
										rope_radius(_rope_radius), segment_len(_segment_len), nLinks(_nLinks) {


	vector<KinBody::LinkPtr> links;
	p_gripperManip->manip->GetChildLinks(links);
	p_finger1 = links[1];
	p_finger2 = links[2];

	btTransform f1Tfm = util::toBtTransform(p_finger1->GetTransform(), METERS);
	btTransform f2Tfm = util::toBtTransform(p_finger2->GetTransform(), METERS);

	p_peg.reset(new CapsuleObject(1, p_radius*METERS, p_len*METERS, f1Tfm, true));

	// Offset from the center of the two transforms
	offset = btVector3((f1Tfm.getOrigin()-f2Tfm.getOrigin()).length()/2,
						0.021*METERS,
						p_len/2*METERS);

	// To orient the peg properly
	corrRot.setValue(0,0,1,0,1,0,-1,0,0);

	_scene->env->add(p_peg);
	p_peg->setColor(0,1,1,1.0); // set the peg's color
	scene.addPreStepCallback(boost::bind(&CustomScene::SuturingPeg::setFingerTransformCallback, this));

	//------------------------ initialize the rope -------------------------------
	vector<btVector3> ctrlPts;

	btVector3 handlePos = getPegCenterTransform().getOrigin();
	vector<btScalar> lengths;
	for (int i=0; i< nLinks; i++)
		//ctrlPts.push_back(handlePos + METERS*btVector3(segment_len*i - 0.15,0,2*rope_radius));  // horizontal rope
		//ctrlPts.push_back(handlePos + METERS*btVector3(0, segment_len*(i - nLinks/2.0), 2*rope_radius)); //vertical rope
		ctrlPts.push_back(handlePos + METERS*btVector3(-segment_len*i, 0, 2*rope_radius)); //vertical rope

	ropePtr.reset(new CapsuleRope(ctrlPts,METERS*rope_radius));
	scene.env->add(ropePtr);
	ropePtr->setColor(0,1,0,1);
	ropePtr->children[0]->setColor(1,0,0,1);
	ropePtr->children[ropePtr->children.size()-1]->setColor(0,0,1,1);

	btTransform TRel;
	TRel.setIdentity();
	TRel.setOrigin(-p_len/2*btVector3(1,0,0)*METERS);
	btTransform TCom = ropePtr->children[0]->rigidBody->getCenterOfMassTransform();

	peg_rope_grab = new Grab (	ropePtr->children[0]->rigidBody.get(), TCom*TRel,
								btVector3(0,0,0), btVector3(0,0,0),
								btVector3(0,0,0), btVector3(1,1,1),
								scene.env->bullet->dynamicsWorld);
	scene.addPreStepCallback(boost::bind(&CustomScene::SuturingPeg::setConnectedRopeTransformCallback, this));
	//------------------------------------------------------------------------------
}

btTransform CustomScene::SuturingPeg::getPegCenterTransform() {return p_peg->getIndexTransform(0);}

void CustomScene::SuturingPeg::getRopePoints (bool nodes, vector<btVector3> & ropePoints, float scale) {
	if (nodes) ropePoints = ropePtr->getNodes();
	else ropePoints = ropePtr->getControlPoints();

	for (int i = 0; i < ropePoints.size(); ++i) ropePoints[i]  = ropePoints[i]*scale;
}

void CustomScene::SuturingPeg::setFingerTransformCallback() {
	btTransform fTfm = (p_grasping_finger1 ? util::toBtTransform(p_finger1->GetTransform(), METERS) : util::toBtTransform(p_finger2->GetTransform(), METERS));
	fTfm.setOrigin(fTfm*offset);
	fTfm.setBasis(fTfm.getBasis()*corrRot);
	p_peg->motionState->setKinematicPos(fTfm);
}

void CustomScene::SuturingPeg::setConnectedRopeTransformCallback() {
	peg_rope_grab->updatePosition(getPegCenterTransform().getOrigin());
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////CUSTOM SCENE///////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/** small test to test the smooth planning of IK planner. */
void CustomScene::moveEndEffector(char dir, bool world, char lr, float step) {
	assert(("unknown direction to move in; should be in {'f','b','u','d','l','r'}",
			(dir=='f'||dir=='b'||dir=='u'||dir=='d'||dir=='r'||dir=='l')));

	IKInterpolationPlanner::Ptr ikPlanner = (lr=='l')? ikPlannerL : ikPlannerR;

	std::pair<bool, RaveTrajectory::Ptr> res = (world)? ikPlanner->goInWorldDirection(dir,step) : ikPlanner->goInDirection(dir,step);
	if (res.first) {
		ravens.controller->appendTrajectory(res.second);
		ravens.controller->run();
	} else {
		std::cout<<"Plan failed!"<<std::endl;
	}
}


void CustomScene::getBoxPoints(vector<btVector3> & boxPoints, float scale) {
	boxPoints.clear();


	//cloth1: i = 0
	vector< pair<int,int> > indices1(bcm);
	for (int i = 0; i < bcm; ++i) indices1[i] = make_pair(0,i);
	cloth1->getBoxClothPoints(indices1, boxPoints);


	//cloth2: i = n-1
	vector< pair<int,int> > indices2(bcm);
	for (int i = 0; i < bcm; ++i) indices2[i] = make_pair(bcn-1,i);
	cloth2->getBoxClothPoints(indices2, boxPoints);

	for (int i = 0; i < boxPoints.size(); ++i) boxPoints[i]  = boxPoints[i]*scale;

}

void CustomScene::getBoxHoles(vector<btVector3> & boxHoles, float scale) {
	boxHoles.clear();
	cloth1->getBoxClothHoles(boxHoles);
	cloth2->getBoxClothHoles(boxHoles);

	for (int i = 0; i < boxHoles.size(); ++i) boxHoles[i]  = boxHoles[i]*scale;
}


// Record rope points to file
void CustomScene::recordPoints () {

	cout<<"Recording points to file."<<endl;

	ostringstream message;
	message << "section";

	vector<btVector3> ropePoints;
	sPeg->getRopePoints(true, ropePoints, 1.0/METERS);
	message << "\nrope ";
	for (int i = 0; i < ropePoints.size(); ++i)
		message << ropePoints[i].x() << " " << ropePoints[i].y() << " " << ropePoints[i].z() << " | ";


	//vector<btVector3> needlePoints;
	//sNeedle->getNeedlePoints(needlePoints, 1.0/METERS);
	//message << "\nneedle ";
	//for (int i = 0; i < needlePoints.size(); ++i)
	//	message << needlePoints[i].x() << " " << needlePoints[i].y() << " " << needlePoints[i].z() << " | ";


	vector<btVector3> boxPoints;
	getBoxPoints(boxPoints, 1.0/METERS);
	message << "\nbox ";
	for (int i = 0; i < boxPoints.size(); ++i)
		message << boxPoints[i].x() << " " << boxPoints[i].y() << " " << boxPoints[i].z() << " | ";


	vector<btVector3> boxHoles;
	getBoxHoles(boxHoles, 1.0/METERS);
	message << "\nhole ";
	for (int i = 0; i < boxHoles.size(); ++i)
		message << boxHoles[i].x() << " " << boxHoles[i].y() << " " << boxHoles[i].z() << " | ";

	cout<<message.str()<<endl;

	jRecorder->addMessageToFile(message.str());
}

// Resets scene
void CustomScene::reset () {
	ravens.setArmPose("home",'b');
	btVector3 handlePos = sPeg->getPegCenterTransform().getOrigin();
	btTransform t;
	for (int i = 0; i < sPeg->ropePtr->nLinks; ++i) {
		sPeg->ropePtr->children[i]->motionState->getWorldTransform(t);
		t.setOrigin(handlePos + METERS*btVector3(-sPeg->segment_len*i, 0, 2*sPeg->rope_radius));
		sPeg->ropePtr->children[i]->rigidBody->setWorldTransform(t);
	}
}


/* Sets up the scene and UI event handlers,
 * initializes various structures.*/
void CustomScene::run() {
	viewer.addEventHandler(new CustomKeyHandler(*this));

	//BulletConfig::internalTimeStep = 0.001;
	const float dt = BulletConfig::dt;

	// setup boost::python for calling lfd functions
	if (RavenConfig::enableLfd)
		setup_python();

	// add a table
	const float table_height = 0.15;
	const float table_thickness = .05;
	table = BoxObject::Ptr(new BoxObject(0, GeneralConfig::scale * btVector3(0.23,0.23,table_thickness/2),
			btTransform(btQuaternion(0, 0, 0, 1),
					GeneralConfig::scale * btVector3(0, 0, table_height-table_thickness/2-0.01))));
	table->receiveShadow = true;

	table->rigidBody->setFriction(0.4);
	env->add(table);
	table->setColor(0.62, 0.32, 0.17, 0.8);
	createKinBodyFromBulletBoxObject(table, rave);
	ravens.ravens->ignoreCollisionWith(table->rigidBody.get());

	// add a needle
	//sNeedle.reset(new SuturingNeedle(this));
	//ravens.ravens->ignoreCollisionWith(sNeedle->s_needle->getChildren()[0]->rigidBody.get());

	//env->add(sNeedle->s_needle);
	//rave->env->Add(sNeedle->s_needle->body);

	// add a peg
	sPeg.reset(new SuturingPeg(this, ravens.manipL));
	ravens.ravens->ignoreCollisionWith(sPeg->p_peg->rigidBody.get());

	// add a cloth
	vector<unsigned int> hole_x, hole_y;

	float bcHeight = table_height+0.015;


	/** Setup rotation perturbation. */
	char axis = RavenConfig::biasAxis;
	btVector3 biasAxis(axis=='x'?1:0, axis=='y'?1:0, axis=='z'?1:0);
	biasAxis.normalize();
	const btMatrix3x3 rotBias(btQuaternion(biasAxis,  RavenConfig::biasAngle));
	const btTransform biasT(rotBias);


	hole_x.push_back(0); hole_x.push_back(0);
	hole_y.push_back(5); hole_y.push_back(2);
	cloth1.reset(new BoxCloth(*this, bcn, bcm, hole_x, hole_y, bcs, bch,
				btVector3( (float)bcn/2*bcs + 0.01 + RavenConfig::xBias, RavenConfig::yBias , bcHeight + RavenConfig::zBias),
				rotBias));
	if (RavenConfig::cloth)
		env->add(cloth1);

	hole_x.clear(); hole_y.clear();
	hole_x.push_back(4); hole_x.push_back(4);
	hole_y.push_back(5); hole_y.push_back(2);
	cloth2.reset(new BoxCloth(*this, bcn, bcm, hole_x, hole_y, bcs, bch,
			btVector3( -(float)bcn/2*bcs - 0.01 + RavenConfig::xBias, RavenConfig::yBias, bcHeight + RavenConfig::zBias)),
			rotBias);
	if (RavenConfig::cloth)
		env->add(cloth2);

	const float support_thickness = .02;
	support1 = BoxObject::Ptr(new BoxObject(0, GeneralConfig::scale * btVector3(0.05,0.05,support_thickness/2),
			biasT*btTransform(btQuaternion(0, 0, 0, 1), GeneralConfig::scale *
			btVector3(-(float)(bcn+1)*bcs - 0.05 + RavenConfig::xBias, 0 + RavenConfig::yBias, bcHeight-support_thickness/2 + RavenConfig::zBias))));

	support2 = BoxObject::Ptr(new BoxObject(0, GeneralConfig::scale * btVector3(0.05,0.05,support_thickness/2),
			biasT*btTransform(btQuaternion(0, 0, 0, 1), GeneralConfig::scale *
			btVector3((float)(bcn+1)*bcs + 0.05 + RavenConfig::xBias, 0 + RavenConfig::yBias, bcHeight-support_thickness/2 + RavenConfig::zBias))));

	support1->rigidBody->setFriction(0.4);
	support2->rigidBody->setFriction(0.4);

	env->add(support1);
	env->add(support2);
	support1->setColor(0.62, 0.32, 0.17, 0.8);
	support2->setColor(0.62, 0.32, 0.17, 0.8);

	// position the ravens
	btTransform T;
	T.setIdentity();
	T.setOrigin(btVector3(0,0,0.05));
	ravens.applyTransform(util::toRaveTransform(T));


	// set up the points for plotting
	plot_points.reset(new PlotPoints(5));
	env->add(plot_points);
	plot_needle.reset(new PlotPoints(10));
	env->add(plot_needle);
	plot_axes1.reset(new PlotAxes());
	env->add(plot_axes1);
	plot_axes2.reset(new PlotAxes());
	env->add(plot_axes2);


	/** Define the actions. */
	vector<CompoundObject<BulletObject>::Ptr> targetsR, targetsL;

	// add the needle, rope, cloth as targets to check for collisions when grabbing.
	//targets.push_back(sNeedle->s_needle);
	targetsL.push_back(sPeg->ropePtr);
	targetsL.push_back(cloth1);
	targetsL.push_back(cloth2);

	char l[] = "l\0";
	char r[] = "r\0";

	lAction.reset(new RavensRigidBodyGripperAction( ravens.manipL,
			"l_grasper2_L",
			"l_grasper1_L",
			env->bullet->dynamicsWorld,
			1, *this, l, jRecorder.get()));
	lAction->setTargets(targetsL);

	targetsR.push_back(sPeg->ropePtr);

	lAction->setPeg(true);
	rAction.reset(new RavensRigidBodyGripperAction( ravens.manipR,
			"r_grasper2_L",
			"r_grasper1_L",
			env->bullet->dynamicsWorld,
			1, *this, r, jRecorder.get()));
	rAction->setTargets(targetsR);


	lAction->setOpenAction();
	runAction(lAction, dt);
	rAction->setOpenAction();
	runAction(rAction, dt);

	ravens.setArmPose("home",'b');

	//setSyncTime(true);
	startViewer();
	stepFor(dt, 2);

	startFixedTimestepLoop(dt);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////TESTS////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/** Plot needle curve-center and tip. */
void CustomScene::plotNeedle (bool remove) {
	plot_needle->setPoints(std::vector<btVector3>(),std::vector<btVector4>());

	btTransform tfm = sNeedle->s_needle->getIndexTransform(0);
	util::drawAxes(sNeedle->getNeedleCenterTransform(),0.2,env);
	std::vector<btVector3> plotpoints;
	std::vector<btVector4> color;

	if (!remove) {
		// COM of needle
		plotpoints.push_back(tfm.getOrigin());
		color.push_back(btVector4(1,0,0,1));

		// Needle tip
		plotpoints.push_back(sNeedle->getNeedleCenterTransform().getOrigin());
		color.push_back(btVector4(0,1,0,1));

		// Needle tip
		plotpoints.push_back(sNeedle->getNeedleTipTransform().getOrigin());
		color.push_back(btVector4(0,0,1,1));

		// Needle handle
		plotpoints.push_back(sNeedle->getNeedleHandleTransform().getOrigin());
		color.push_back(btVector4(1,1,0,1));

		// Middle of grippers, offset distance: 3mm
		plotpoints.push_back(ravens.manipL->getTransform().getOrigin()+ravens.manipL->getTransform().getBasis().getColumn(2)*-0.003*METERS);
		color.push_back(btVector4(1,1,1,1));
		plotpoints.push_back(ravens.manipR->getTransform().getOrigin()+ravens.manipR->getTransform().getBasis().getColumn(2)*-0.003*METERS);
		color.push_back(btVector4(1,1,1,1));

		util::drawAxes(sNeedle->getNeedleHandleTransform(), 0.2*METERS, env);
		util::drawAxes(sNeedle->s_needle->getIndexTransform(0), 0.2*METERS, env);
	}
	else {
		plotpoints.push_back(btVector3(0,0,0));
		color.push_back(btVector4(0,0,0,0));
	}

	util::drawAxes(ravens.manipL->getTransform(),0.5,env);
	util::drawAxes(ravens.manipR->getTransform(),0.5,env);

	plot_needle->setPoints(plotpoints,color);
}


/** Small test for plotting grasping relating things. */
void CustomScene::plotGrasp (bool remove) {
	plot_points->setPoints(std::vector<btVector3>(),std::vector<btVector4>());

	btVector3 pt1 = lAction->getVec(false);
	btVector3 pt2 = lAction->getVec(true);
	btVector3 pt3 = rAction->getVec(false);
	btVector3 pt4 = rAction->getVec(true);
	btVector3 vec1 = rAction->getClosingDirection(true);
	btVector3 vec2 = rAction->getClosingDirection(false);
	btVector3 vec3 = rAction->getClosingDirection(true);
	btVector3 vec4 = rAction->getClosingDirection(false);

	btTransform tfm1 = util::getOrthogonalTransform(vec1);
	tfm1.setOrigin(pt4);
	btTransform tfm2 = util::getOrthogonalTransform(vec2);
	tfm2.setOrigin(pt3);

	btTransform tfm3 = util::getOrthogonalTransform(vec3);
	tfm3.setOrigin(pt2);
	btTransform tfm4 = util::getOrthogonalTransform(vec4);
	tfm4.setOrigin(pt1);

	btTransform tfm5 = lAction->getTfm(true);
	//tfm5.setOrigin(pt2);
	btTransform tfm6 = rAction->getTfm(true);
	//tfm6.setOrigin(pt4);

	btTransform tfm7 = lAction->getTfm(false);
	//tfm7.setOrigin(pt1);
	btTransform tfm8 = rAction->getTfm(false);
	//tfm8.setOrigin(pt3);

	std::vector<btVector3> plotpoints;
	std::vector<btVector4> color;

	if (!remove) {
		plotpoints.push_back(pt2);
		color.push_back(btVector4(1,0,0,1));

		plotpoints.push_back(pt2 + 0.1*vec1);
		color.push_back(btVector4(0,0,1,1));

		plotpoints.push_back(pt4);
		color.push_back(btVector4(1,0,0,1));

		plotpoints.push_back(pt4 + 0.1*vec3);
		color.push_back(btVector4(0,0,1,1));

		//plotpoints.push_back(pt4);
		//color.push_back(btVector4(0,0,1,1));

		//util::drawAxes(tfm7, 2, env);
		//util::drawAxes(tfm8, 2, env);
		//util::drawAxes(lAction->getTfm(true),0.1*METERS, env);
		vector<KinBody::LinkPtr> links;
		ravens.manipR->manip->GetChildLinks(links);
		util::drawAxes(util::toBtTransform(links[1]->GetTransform(), METERS), 0.05*METERS, env);
		util::drawAxes(util::toBtTransform(links[2]->GetTransform(), METERS), 0.05*METERS, env);

		links.clear();
		ravens.manipL->manip->GetChildLinks(links);
		util::drawAxes(util::toBtTransform(links[1]->GetTransform(), METERS), 0.05*METERS, env);
		util::drawAxes(util::toBtTransform(links[2]->GetTransform(), METERS), 0.05*METERS, env);
	}
	else {
		plotpoints.push_back(btVector3(0,0,0));
		color.push_back(btVector4(0,0,0,0));
	}

	plot_points->setPoints(plotpoints,color);

}


void CustomScene::plotAllPoints(bool remove) {
	plot_points->setPoints(std::vector<btVector3>(),std::vector<btVector4>());

	//Get all points
	//vector<btVector3> needlePoints; sNeedle->getNeedlePoints(needlePoints);
	vector<btVector3> ropePoints; sPeg->getRopePoints(true, ropePoints);
	vector<btVector3> boxPoints; getBoxPoints(boxPoints);
	vector<btVector3> boxHoles; getBoxHoles(boxHoles);

	std::vector<btVector3> plotpoints(ropePoints.size() + boxPoints.size() + boxHoles.size());
	std::vector<btVector4> color(ropePoints.size() + boxPoints.size() + boxHoles.size());

	cout<<"Size of ropePoints: "<<ropePoints.size()<<endl;
	//cout<<"Size of needlePoints: "<<needlePoints.size()<<endl;
	cout<<"Size of boxPoints: "<<boxPoints.size()<<endl;
	cout<<"Size of boxHoles: "<<boxHoles.size()<<endl;
	cout<<"Size of plotPoints: "<<plotpoints.size()<<endl;


	if (!remove) {

		unsigned int i = 0;

		//Needle in red
		//for (int j = 0; j < needlePoints.size(); ++j) {
		//	plotpoints[i] = needlePoints[j];
		//	color[i++] = btVector4(1,0,0,1);
		//}

		//Rope in blue
		for (int j = 0; j < ropePoints.size(); ++j) {
			plotpoints[i] = ropePoints[j];
			color[i++] = btVector4(0,0,1,1);
		}

		//Box points in green
		for (int j = 0; j < boxPoints.size(); ++j) {
			plotpoints[i] = boxPoints[j];
			color[i++] = btVector4(0,1,0,1);
		}

		//Needle in yellow
		for (int j = 0; j < boxHoles.size(); ++j) {
			plotpoints[i] = boxHoles[j];
			color[i++] = btVector4(1,1,0,1);
		}
	}
	else {
		plotpoints.push_back(btVector3(0,0,0));
		color.push_back(btVector4(0,0,0,0));
	}

	plot_points->setPoints(plotpoints,color);

}

void CustomScene::plotHoleTfm () {
	util::drawAxes(cloth1->holes[0]->getIndexTransform(0), 0.2*METERS, env);
	util::drawAxes(cloth1->children[0]->getIndexTransform(0), 0.2*METERS, env);
}

void CustomScene::plotPeg () {
	//util::drawAxes(sPeg->p_peg->getIndexTransform(0), 0.5*METERS, env);
	//util::drawAxes(sPeg->ropePtr->children[0]->getIndexTransform(0), 0.5*METERS, env);
}

/** Small test to see the angles of the ends of the needle.*/
void CustomScene::testNeedle() {
	btTransform tfm = sNeedle->getNeedleCenterTransform();
	btVector3 c = tfm.getOrigin();
	btVector3 ch = sNeedle->getNeedleHandleTransform().getOrigin();
	btVector3 ct = sNeedle->getNeedleTipTransform().getOrigin();
	btVector3 xVec = tfm.getBasis().getColumn(0);

	btVector3 hVec = (ch-c).normalized();
	btVector3 tVec = (ct-c).normalized();

	cout<<"Dot with handle: "<<hVec.dot(xVec)<<endl;
	cout<<"Dot with tip: "<<tVec.dot(xVec)<<endl;
}

/** Test circular. */
void CustomScene::testNeedle2 () {
	btVector3 rPt = ravens.manipR->getTransform().getOrigin();
	btVector3 lPt = ravens.manipL->getTransform().getOrigin();

	plot_needle->setPoints(std::vector<btVector3>(),std::vector<btVector4>());
	std::vector<btVector3> plotpoints;
	std::vector<btVector4> color;
	plotpoints.push_back(lPt);
	color.push_back(btVector4(1,0,0,1));
	plotpoints.push_back(rPt);
	color.push_back(btVector4(0,1,0,1));
	plot_needle->setPoints(plotpoints,color);

	if (sNeedle->pointCloseToNeedle(rPt))
		cout << "Rgripper close to needle. "<<endl;

	if (sNeedle->pointCloseToNeedle(lPt))
		cout << "Lgripper close to needle. "<<endl;

	vector<btVector3> vec;
	sNeedle->getNeedlePoints(vec);
	for (int i = 0; i < vec.size(); ++i) cout<<sNeedle->pointCloseToNeedle(vec[i])<<endl;
}


/** test grabbing of needle. */
void CustomScene::testGrab() {
	btVector3 rPt = ravens.manipR->getTransform().getOrigin();
	btVector3 lPt = ravens.manipL->getTransform().getOrigin();

	if (sNeedle->pointCloseToNeedle(rPt)) {
		vector<KinBody::LinkPtr> links;
		ravens.manipR->manip->GetChildLinks(links);
		ravens.ravens->robot->Grab(sNeedle->s_needle->body, links[0]);
		cout<<"Grabbing with R!"<<endl;
	} else if (sNeedle->pointCloseToNeedle(lPt)) {
		vector<KinBody::LinkPtr> links;
		ravens.manipL->manip->GetChildLinks(links);
		ravens.ravens->robot->Grab(sNeedle->s_needle->body, links[0]);
		cout<<"Grabbing with L!"<<endl;
	}
}


/** small test to test the controller. */
void CustomScene::testTrajectory() {

	ravens.ravens->robot->SetActiveManipulator("r_arm");

	ModuleBasePtr basemodule = RaveCreateModule(rave->env,"BaseManipulation");
	rave->env->Add(basemodule,true,ravens.ravens->robot->GetName());

	Transform rightT = ravens.manipR->manip->GetEndEffectorTransform();
	rightT.trans += Vector(0.0,0,-0.02);

	// see if trajectory can be extracted: Yes it can be!
	TrajectoryBasePtr hand_traj;
	hand_traj = RaveCreateTrajectory(rave->env,"");

	stringstream ssout, ssin; ssin << "MoveToHandPosition outputtraj execute 0 poses 1  " << rightT;
	basemodule->SendCommand(ssout,ssin);
	hand_traj->deserialize(ssout);

	printf("TRAJECTORY INFO : DURATION : %f\n",(float) hand_traj->GetDuration());

	RaveTrajectory::Ptr traj(new RaveTrajectory(hand_traj, ravens.ravens, ravens.manipR->manip->GetArmIndices()));
	ravens.controller->appendTrajectory(traj);
	ravens.controller->run();
}


/** small test to test the planner and the controller. */
void CustomScene::testTrajectory2() {
	//WayPointsPlanner planner1(pr2m.pr2, rave, 'r');
	IKInterpolationPlanner ikPlanner(ravens, rave, 'r');


	ravens.ravens->robot->SetActiveManipulator("r_arm");
	Transform rightT = ravens.manipR->manip->GetEndEffectorTransform();

	std::vector<Transform> t;

	//visualize
	plotcolors.clear();
	plotpoints.clear();

	for(int i =0; i < 10; i+=1) {
		Transform t1 = rightT;
		t1.trans += Vector(0.0001*(i+1)*(i+1),0,-0.005*(i+1));

		t.push_back(t1);

		// visual aid
		plotpoints.push_back(util::toBtVector(GeneralConfig::scale*t1.trans));
		plotcolors.push_back(btVector4(2,0,0,1));
	}
	plot_points->setPoints(plotpoints,plotcolors);

	std::pair<bool, RaveTrajectory::Ptr> res = ikPlanner.smoothPlan(t);
	if (res.first) {
		ravens.controller->appendTrajectory(res.second);
		ravens.controller->run();
	} else {
		std::cout<<"Plan failed!"<<std::endl;
	}
}

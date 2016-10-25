#include <iostream>
#include <math.h>

#include "scene_physics_engine_w_rendering.h"

PhysicsEngineWRender::PhysicsEngineWRender() : have_background_(false), debug_messages_(false), rendering_launched_(false)
{
	if (this->debug_messages_) std::cerr << "Setting up physics engine.\n";
	this->initPhysics();
}

void PhysicsEngineWRender::addBackgroundPlane(const std::vector<btVector3> &plane_points)
{
	// mtx_.lock();

	if (this->debug_messages_) std::cerr << "Adding background(convex hull) to the physics engine's world.\n";
	
	btVector3 plane_center(0,0,0);

	btConvexHullShape background_convex;
	for (std::vector<btVector3>::const_iterator it = plane_points.begin(); it != plane_points.end(); ++it)
	{
		// add a point to the convex hull then recalculate the AABB
		background_convex.addPoint(*it, true);
		plane_center+= *it;
	}

	plane_center /= plane_points.size();
	// since everything is in reference to camera coordinate, camera coordinate is 0,0,0
	this->camera_coordinate_ = btVector3(0,0,0);
	this->target_coordinate_ = plane_center;
	this->setCameraPositionAndTarget(camera_coordinate_,target_coordinate_);
	this->setCameraClippingPlaneNearFar(0.005f);

	btCollisionShape*  background = new btConvexHullShape(background_convex);
	btDefaultMotionState* background_motion_state = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
	
	// unmovable ground object
	btRigidBody::btRigidBodyConstructionInfo
			background_RigidBodyCI(0, background_motion_state, background, btVector3(0, 0, 0));
	
	this->background_ = new btRigidBody(background_RigidBodyCI);
	this->background_->setFriction(1.f);
	this->background_->setRollingFriction(1.f);
	
	m_dynamicsWorld->addRigidBody(this->background_);

	if (this->debug_messages_)
	{
		std::cerr << "Background: \n";
		btTransform background_tf;
		this->background_->getMotionState()->getWorldTransform(background_tf);
		btQuaternion q = background_tf.getRotation();
		btVector3 t = background_tf.getOrigin();
		std::cerr << "Quaterion: " << q[0] << ", " << q[1] << ", "  << q[2] << ", "  << q[3] << std::endl;
		std::cerr << "Translation: " << t[0]  << ", " << t[1]  << ", " << t[2] << std::endl;
	}
	this->have_background_ = true;

	// mtx_.unlock();
	btCollisionShape* fallShape = new btSphereShape(0.05);
	btDefaultMotionState* fallMotionState =
            new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), this->target_coordinate_));
    btScalar mass = 0;
    btVector3 fallInertia(0, 0, 0);
    fallShape->calculateLocalInertia(mass, fallInertia);
    btRigidBody::btRigidBodyConstructionInfo fallRigidBodyCI(mass, fallMotionState, fallShape, fallInertia);
    btRigidBody* fallRigidBody = new btRigidBody(fallRigidBodyCI);
    m_dynamicsWorld->addRigidBody(fallRigidBody);
    m_collisionShapes.push_back(fallShape);
    m_collisionShapes.push_back(background);
}

void PhysicsEngineWRender::addBackgroundMesh()
{
	if (this->debug_messages_) std::cerr << "Adding background(mesh).\n";
	// TODO
	this->have_background_ = true;
}

void PhysicsEngineWRender::setGravityVectorDirectionFromTfYUp(const btTransform &transform_y_is_inverse_gravity_direction)
{
	if (this->debug_messages_) std::cerr << "Setting physics engine gravity based on a TF transform.\n";

	btMatrix3x3 rotation_matrix = transform_y_is_inverse_gravity_direction.getBasis();
	if (this->debug_messages_)
	{
		std::cerr << "Input transform:\n";
		for (int i = 0; i < 3; i++){
			for (int j = 0; j < 3; j++)
			{
				std::cerr <<  rotation_matrix[i][j] << " ";
			}
			std::cerr << std::endl;
		}

	} 

	// get Z axis direction from the transform
	btVector3 inverse_gravity(rotation_matrix[0][2],rotation_matrix[1][2],rotation_matrix[2][2]);
	
	this->setGravityVectorDirection(-inverse_gravity);
}

void PhysicsEngineWRender::setGravityVectorDirection(const btVector3 &gravity)
{
	// mtx_.lock();
	if (this->debug_messages_) std::cerr << "Setting physics engine gravity vector.\n";
	btVector3 gravity_corrected_magnitude = gravity / gravity.norm() * 9.807;
	if (this->debug_messages_) std::cerr << "Gravity vector::" << gravity_corrected_magnitude[0] << ", "
		<< gravity_corrected_magnitude[1] << ", " 
		<< gravity_corrected_magnitude[2] << std::endl;
	m_dynamicsWorld->setGravity(gravity_corrected_magnitude);

	// mtx_.unlock();
}

void PhysicsEngineWRender::addObjects(const std::vector<ObjectWithID> &objects)
{
	// mtx_.lock();
	// Add new objects
	if (this->debug_messages_) std::cerr << "Adding scene objects to the physics engine.\n";
	
	for (std::vector<ObjectWithID>::const_iterator it = objects.begin(); 
		it != objects.end(); ++it)
	{
		if (this->debug_messages_) std::cerr << "Adding rigid body " << it->getID() << " to the physics engine's database.\n";
		this->rigid_body_[it->getID()] = it->generateRigidBodyForWorld();
		if (this->debug_messages_) std::cerr << "Adding rigid body " << it->getID() << " to the physics engine's world.\n";
		m_dynamicsWorld->addRigidBody(this->rigid_body_[it->getID()]);

		if (this->debug_messages_)
		{
			std::cerr << "Object " << it->getID() <<", address: " << this->rigid_body_[it->getID()]  <<": \n";
			btTransform object_tf;
			this->rigid_body_[it->getID()]->getMotionState()->getWorldTransform(object_tf);
			btQuaternion q = object_tf.getRotation();
			btVector3 t = object_tf.getOrigin();
			std::cerr << "Quaterion: " << q[0] << ", " << q[1] << ", "  << q[2] << ", "  << q[3] << std::endl;
			std::cerr << "Translation: " << t[0]  << ", " << t[1]  << ", " << t[2] << std::endl;
		}
	}
	if (this->debug_messages_) std::cerr << "Scene objects added to the physics engine.\n";

	// mtx_.unlock();
}

void PhysicsEngineWRender::simulate()
{
	// mtx_.lock();

	if (this->debug_messages_) std::cerr << "Simulating the physics engine.\n";
	// If do not have background, do not update the scene
	
	if (!this->have_background_)
	{
		std::cerr << "Skipping simulation, scene does not has any background data yet.\n";
		return;
	}

	// TODO: early termination. Check if all object has no changes anymore.
	bool steady_state = false;

	for (int i = 0; i < 300; i++)
	{
		// if (this->rendering_launched_)
		// 	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
		///step the simulation
		if (m_dynamicsWorld)
		{
			m_dynamicsWorld->stepSimulation(1 / 60.f, 10);
			//optional but useful: debug drawing
			m_dynamicsWorld->debugDrawWorld();

			if (steady_state) break;
		}
		
		// if (this->rendering_launched_){
		// 	renderme(); 
		// 	glFlush();
		// 	swapBuffers();
		// }
	}
	if (this->debug_messages_) std::cerr << "Simulation done.\n";
	// mtx_.unlock();
}

std::map<std::string, btTransform>  PhysicsEngineWRender::getUpdatedObjectPose()
{
	if (this->debug_messages_) std::cerr << "Getting updated scene objects poses.\n";
	// Run simulation to get an update on object poses
	// this->simulate();

	// mtx_.lock();
	std::map<std::string, btTransform> result_pose;
	for (std::map<std::string, btRigidBody*>::const_iterator it = this->rigid_body_.begin(); 
		it != this->rigid_body_.end(); ++it)
	{
		it->second->getMotionState()->getWorldTransform(result_pose[it->first]);

		if (this->debug_messages_)
		{
			std::cerr << "Object " << it->first << std::endl;
			btQuaternion q = result_pose[it->first].getRotation();
			btVector3 t = result_pose[it->first].getOrigin();
			std::cerr << "Quaterion: " << q[0] << ", " << q[1] << ", "  << q[2] << ", "  << q[3] << std::endl;
			std::cerr << "Translation: " << t[0]  << ", " << t[1]  << ", " << t[2] << std::endl;
		}
	}

	if (this->debug_messages_) std::cerr << "Done scene objects poses.\n";

	// mtx_.unlock();

	return result_pose;
}

void PhysicsEngineWRender::resetObjects()
{
	// mtx_.lock();
	if (this->debug_messages_) std::cerr << "Removing all scene objects.\n";
	// Removes all objects from the physics world then delete its' content
	for (std::map<std::string, btRigidBody*>::iterator it = this->rigid_body_.begin(); 
		it != this->rigid_body_.end(); ++it)
	{
		m_dynamicsWorld->removeRigidBody(it->second);
		delete it->second->getMotionState();
		delete it->second;
		if (this->debug_messages_) std::cerr << "Removed objects: "<<  it->first <<".\n";
	}
	this->rigid_body_.clear();
	if (this->debug_messages_) std::cerr << "Done removing all scene objects.\n";

	// mtx_.unlock();
}

void PhysicsEngineWRender::setDebugMode(bool debug)
{
	this->debug_messages_ = debug;
}


void PhysicsEngineWRender::renderingLaunched()
{
	this->rendering_launched_ = true;
}

void PhysicsEngineWRender::initPhysics()
{
	setTexturing(true);
	setShadows(true);
	m_broadphase = new btDbvtBroadphase();
	m_collisionConfiguration = new btDefaultCollisionConfiguration();
	m_dispatcher = new btCollisionDispatcher(m_collisionConfiguration);
	m_solver  = new btSequentialImpulseConstraintSolver;
	m_dynamicsWorld = new btDiscreteDynamicsWorld(m_dispatcher, 
		m_broadphase, m_solver, m_collisionConfiguration);
	m_dynamicsWorld->setDebugDrawer(&gDebugDraw);
}

void PhysicsEngineWRender::exitPhysics()
{
	// Clean up pointers
	if (this->debug_messages_) std::cerr << "Shutting down physics engine.\n";
	// delete all object contents
	this->resetObjects();
	
	// removes background
	if (this->have_background_)
	{
		m_dynamicsWorld->removeRigidBody(this->background_);
		delete this->background_->getMotionState();
		delete this->background_;
	}

	// delete physics world environment
	if (this->debug_messages_) std::cerr << "Deleting physics engine environment.\n";
	delete m_dynamicsWorld;
	delete this->m_solver;
	delete this->m_dispatcher;
	delete this->m_collisionConfiguration;
	delete this->m_broadphase;
}

void	PhysicsEngineWRender::clientResetScene()
{
	exitPhysics();
	initPhysics();
}

void PhysicsEngineWRender::clientMoveAndDisplay()
{
	// mtx_.lock();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

	//simple dynamics world doesn't handle fixed-time-stepping
	float ms = getDeltaTimeMicroseconds();
	
	///step the simulation
	if (m_dynamicsWorld)
	{
		m_dynamicsWorld->stepSimulation(ms / 1000000.f);
		//optional but useful: debug drawing
		m_dynamicsWorld->debugDrawWorld();
	}
	// std::cerr << "Number of objects: " << this->rigid_body_.size() << std::endl;
	
	renderme(); 
	glFlush();
	swapBuffers();
	// mtx_.unlock();
}

void PhysicsEngineWRender::displayCallback(void) {

	// mtx_.lock();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
	renderme();

	//optional but useful: debug drawing to detect problems
	if (m_dynamicsWorld)
		m_dynamicsWorld->debugDrawWorld();

	glFlush();
	swapBuffers();

	// mtx_.unlock();
}


void PhysicsEngineWRender::setCameraClippingPlaneNearFar(btScalar near, btScalar far)
{
	this->m_frustumZNear = near;
	this->m_frustumZFar = far;
}

void PhysicsEngineWRender::setCameraPositionAndTarget(btVector3 cam_position, btVector3 cam_target)
{
	this->m_cameraPosition = cam_position;
	this->m_cameraTargetPosition = cam_target;
	m_cameraDistance = m_cameraTargetPosition.distance(m_cameraPosition);
	
	// calculate polar coordinate of the camera
	btVector3 target_to_cam_direction = (m_cameraPosition - m_cameraTargetPosition).normalize();
	this->m_ele = 90 - acos(target_to_cam_direction[1]) * 57.29577951308232;
	this->m_azi = atan2(-target_to_cam_direction[0],-target_to_cam_direction[2]) * 57.29577951308232;
}


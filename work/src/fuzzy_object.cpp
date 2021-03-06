//---------------------------------------------------------------------------
// Particle Representation of a 3D Object
//
// By Jack Purvis
//
// Referenced paper:
// “Obtaining Fuzzy Representations of 3D Objects”, by Brent M. Dingle, November 2005.
// https://engineering.tamu.edu/media/697054/tamu-cs-tr-2005-11-6.pdf
//---------------------------------------------------------------------------

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "cgra_math.hpp"
#include "cgra_geometry.hpp"
#include "fuzzy_object.hpp"
#include "geometry.hpp"
#include "opengl.hpp"

using namespace std;
using namespace cgra;

FuzzyObject::FuzzyObject(Geometry *geometry) {
	g_geometry = geometry;

	setupDisplayList();

	spawnPoint = geometry->getOrigin();

	// Use this to build the complete system upon object instantiation
	//buildSystem(false);
}

FuzzyObject::~FuzzyObject() {}

// Setup the particle instance geometry
void FuzzyObject::setupDisplayList() {

	// Delete the old list if there is one
	if (p_displayList) glDeleteLists(p_displayList, 1);

	// Setup the new display list
	p_displayList = glGenLists(1);
	glNewList(p_displayList, GL_COMPILE);

	// Draw the geometry
	cgraSphere(p_radius, 6, 6);

	glEndList();
}

// Iterate one step through the system building algorithm
void FuzzyObject::buildSystemIncrement() {
	buildSystem(true);
}

// Build the particle system
void FuzzyObject::buildSystem(bool incremental) {
	// First pass
	while (!firstPassFinished && !stoppingCriteria()) {
		addParticle();
		updateBuildingSystem();

		// Return now if we only wanted to build one step
		if (incremental) return;
	}

	// Final pass
	while (!systemAtRest()) {
		updateBuildingSystem();

		// Return now if we only wanted to build one step
		if (incremental) return;
	}

	buildFinished = true;
}

// Determines when the particle system has been fully generated
bool FuzzyObject::stoppingCriteria() {
	if (particles.size() >= particleLimit) return true;

	// If all the particles are in collision
	if (collisionCount == particles.size() && particles.size() > minParticleCount) {

		// Perform a number of stability updates
		for (int i = 0; i < stabilityUpdates; i++) {
			updateBuildingSystem();
		}

		// If the system is still unstable then the build process is complete
		if (collisionCount == particles.size() && particles.size() > minParticleCount) {
			firstPassFinished = true;
			return true;
		}
	}

	return false;
}

// Determines when the system has gained stability after all the particles have been added
bool FuzzyObject::systemAtRest() {

	// Not currently checking this yet
	return true;
}

// Add a new particle to the system
void FuzzyObject::addParticle() {

	// Do not add another particle if we reached the limit
	if (particles.size() >= particleLimit) return;

	fuzzyParticle p;
	p.pos = vec3(spawnPoint.x + math::random(-p_spawnOffset, p_spawnOffset),
							 spawnPoint.y + math::random(-p_spawnOffset, p_spawnOffset),
							 spawnPoint.z + math::random(-p_spawnOffset, p_spawnOffset));

	p.acc = vec3(0.0f, 0.0f, 0.0f);

	// Random velocity generation
	p.vel = vec3(math::random(-1.0f, 1.0f) * p_velRange,
							 math::random(-1.0f, 1.0f) * p_velRange,
							 math::random(-1.0f, 1.0f) * p_velRange);

	p.col = vec3(1.0f, 1.0f, 1.0f);

	p.id = nextUniqueId++;

	particles.push_back(p);

	updateFacingTriangle(particles.size() - 1);
}

// Perform one update step in the system building process
void FuzzyObject::updateBuildingSystem() {
	collisionCount = 0;

	// Reset required fields on each particle for the next update
	for (int i = 0; i < particles.size(); i++) {
		particles[i].acc = vec3(0.0f, 0.0f, 0.0f);
		particles[i].inCollision = false;

		// Check if the particle left the mesh
		//if (!g_geometry->pointInsideMesh(particles[i].pos)) {
		float d = dot(particles[i].pos - particles[i].triangleIntersectionPos, -g_geometry->getSurfaceNormal(particles[i].triangleIndex));
		if (d < 0.0f || d >= maxFloatVector.x) {

			// Mark it for deletion
			particlesForDeletion.push_back(i);
		}
	}

	// Delete any particles marked for deletion
	if (particlesForDeletion.size() > 0) {
		vector<fuzzyParticle> newParticles;

		for (int i = 0; i < particles.size(); i++) {
			if (find(particlesForDeletion.begin(), particlesForDeletion.end(), i) == particlesForDeletion.end()) {
				newParticles.push_back(particles[i]);
			}
		}

		particlesForDeletion.clear();
		particles = newParticles;
	}

	// Apply LJ physics based forces to the particle system
	applyParticleForces();

	// Apply forces to particles that collide with the mesh geometry
	applyBoundaryForces();

	// Update the particle positions and velocities
	for (int i = 0; i < particles.size(); i++) {
		particles[i].acc /= p_mass;
		particles[i].vel = clamp(particles[i].vel + particles[i].acc, -p_velRange, p_velRange);
		particles[i].pos += particles[i].vel;

		// If the particle accelerated it has potentially changed direction
		if (particles[i].acc.x != 0.0f && particles[i].acc.y != 0.0f && particles[i].acc.z != 0.0f) {

			// Recompute the particle facing triangle
			updateFacingTriangle(i);
		}

		if (particles[i].inCollision) collisionCount++;
	}
}

// Apply forces between particles
void FuzzyObject::applyParticleForces() {

	// For each pair of particles
	for (int i = 0; i < particles.size(); i++) {
		for (int j = i + 1; j < particles.size(); j++) {

			// If the particles are within the effect range of eachother we count this as a collision
			if (withinRange(particles[i].pos, particles[j].pos, e_effectRange)) {

				// Compute the distance between particles
				vec3 distVector = particles[i].pos - particles[j].pos;
				float dist = length(distVector);

				if (dist < 0.001f) continue; // Prevent dividing by 0 effects

				// Compute and apply the force both particles exert on eachother
				vec3 force = forceAtDistance(dist, distVector);
				particles[i].acc += force;
				particles[j].acc -= force;

				// Apply friction to the particle
				particles[i].vel *= particleCollisionFriction;
				particles[j].vel *= particleCollisionFriction;

				particles[i].inCollision = true;
				particles[j].inCollision = true;
			}
		}
	}
}

// Apply forces to particles if they are colliding with the mesh geometry
void FuzzyObject::applyBoundaryForces() {
	// For each particle
	for (int i = 0; i < particles.size(); i++) {

		// If the particle is colliding with the intersection point
		if (withinRange(particles[i].pos, particles[i].triangleIntersectionPos, p_boundaryRadius)) {
		 	// Bounce the particle off the triangle surface by reflecting it's velocity
			particles[i].vel = reflect(particles[i].vel, -(g_geometry->getSurfaceNormal(particles[i].triangleIndex))) * meshCollisionFriction;
			particles[i].acc = vec3(0.0f, 0.0f, 0.0f);

			// The particle is now facing the opposite direction so the facing triangle must be recomputed
			updateFacingTriangle(i);
		}
	}
}

// Returns the force that should be applied to two particles at a given distance
//  between each other, based on the Lennard Jones potentials model
vec3 FuzzyObject::forceAtDistance(float dist, vec3 distVector) {
	float a = (48 * e_strength / pow(e_lengthScale, 2));
	float b = pow(e_lengthScale / dist, 14);
	float c = 0.5f * pow(e_lengthScale / dist, 8);
	return a * (b - c) * distVector;
}

// Use the square distance to cut costs by avoiding square roots
bool FuzzyObject::withinRange(vec3 p1, vec3 p2, float range) {
	vec3 d = p1 - p2;
	return (d.x * d.x + d.y * d.y + d.z * d.z) < (range * range);
}

// Recompute the triangle the given particle is facing so collisions can be checked against it
void FuzzyObject::updateFacingTriangle(int index) {
	vec3 newIntersectionPoint = vec3(maxFloatVector);
	float shortestLength = maxFloatVector.x;
	int triangleIndex = 0;

	// For each triangle
	for (int i = 0; i < g_geometry->triangleCount(); i++) {

		// Using the particle velocity as the direction vector
		// Compute the intersection point on the triangle
		vec3 intersectionPoint = (g_geometry->rayIntersectsTriangle(particles[index].pos, particles[index].vel, i));

		// Skip this triangle if no intersection occured
		if (intersectionPoint.x == maxFloatVector.x) continue;

		// If this is the closest intersection point yet
		if (withinRange(particles[index].pos, intersectionPoint, shortestLength)) {
			newIntersectionPoint = intersectionPoint;
			shortestLength = length(particles[index].pos - newIntersectionPoint);
			triangleIndex = i;
		}
	}

	// Assign the final closest intersection point
	particles[index].triangleIntersectionPos = newIntersectionPoint;
	particles[index].triangleIndex = triangleIndex;

	particles[index].inCollision = true;
}

void FuzzyObject::renderSystem() {
	glPushMatrix();

	// Translate to the geometry position
	vec3 geometry_pos = g_geometry->getPosition();
	glTranslatef(geometry_pos.x, geometry_pos.y, geometry_pos.z);

	// Draw the spawn position
	// glDisable(GL_LIGHTING);
	// glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, vec4(1.0f, 0.0f, 0.0f, 1.0f).dataPointer());
	// glPointSize(4);
	// glBegin(GL_POINTS);
	// glVertex3f(spawnPoint.x, spawnPoint.y, spawnPoint.z);
	// glEnd();
	// glEnable(GL_LIGHTING);

	// Set particle material properties
	glMaterialfv(GL_FRONT, GL_SPECULAR, specular.dataPointer());
	glMaterialfv(GL_FRONT, GL_SHININESS, &shininess);

	// Set particle drawing properties
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glLineWidth(1);

	for (int i = 0; i < particles.size(); i++) {
		fuzzyParticle p = particles[i];

		glPushMatrix();
		glTranslatef(p.pos.x, p.pos.y, p.pos.z);

		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, p.col.dataPointer());

		// Draw the particle
		glCallList(p_displayList);

		glPopMatrix();
	}

	glPopMatrix();

	glEnd();
}

int FuzzyObject::getParticleCount() {
	return particles.size();
}

bool FuzzyObject::finishedBuilding() {
	return buildFinished;
}

// Return the complete particle system as a collection of point vectors
vector<vec3> FuzzyObject::getSystem() {
	vector<vec3> points;

	for (int i = 0; i < particles.size(); i++) {
		points.push_back(particles[i].pos);
	}

	return points;
}

void FuzzyObject::clearParticles() {
	particles.clear();
}

// Scales the algorithm paramaters to adjust the density of the resulting
// particle system in a linear fashion
void FuzzyObject::scaleDensity(float amount) {
	p_radius *= amount;
	p_boundaryRadius *= amount;
	p_spawnOffset *= amount;
	e_lengthScale = min(e_lengthScale, e_lengthScale * max(amount * 1.5f, 1.0f));
	e_effectRange = pow(2.0f, 1.0f / 6.0f) * e_lengthScale;

	setupDisplayList();
}

// Used to hard code in some nice values for converting models quickly and fairly accurately
void FuzzyObject::setExampleSystemAttributes() {
	stabilityUpdates = 10;
	p_velRange = 0.03f;
	p_radius = 0.2f;
	p_boundaryRadius = 0.23f;
	p_spawnOffset = 0.05f;
	e_strength = 0.005f;
	e_lengthScale = 0.32f;
	e_effectRange = pow(2.0f, 1.0f / 6.0f) * e_lengthScale;

	setupDisplayList();
}

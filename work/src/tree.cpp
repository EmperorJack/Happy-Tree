#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "cgra_geometry.hpp"
#include "cgra_math.hpp"
#include "opengl.hpp"
#include "tree.hpp"


using namespace std;
using namespace cgra;

Tree::Tree(){
	root = makeDummyTree(4);
	setWindForce(vec3(20,0,20));
}

branch* Tree::makeDummyTree(int numBranches){
	branch* b = new branch();
	b->name = "trunk"+to_string(numBranches);
	b->direction = vec3(0,1,0);
	b->offset = math::random(0.0f,0.1f);
	b->length = length;
	b->baseWidth = width * numBranches;
	b->topWidth = (width * (numBranches - 1));
	if(numBranches == 1){
		b->topWidth = (width /2);
	}

	b->basisRot = vec3(0,0,0);
	if(numBranches > 1){

		for (int i = 0; i < 4; i++){
			branch* c = new branch();

			if(i == 0){
				c->name = "branch0 trunk"+to_string(numBranches);
				c->direction = vec3(1,0.3,0);
			}else if(i == 1){
				c->name = "branch1 trunk"+to_string(numBranches);
				c->direction = vec3(-1,0.3,0);
			}else if(i == 2){
				c->name = "branch2 trunk"+to_string(numBranches);
				c->direction = vec3(0,0.3,1);
			}else if(i == 3){
				c->name = "branch3 trunk"+to_string(numBranches);
				c->direction = vec3(0,0.3,-1);
			}

			c->length = length/2 * (numBranches-1);
			c->baseWidth = width * (numBranches-1);
			c->topWidth = width/2;
			c->basisRot = vec3(0,0,0);


			b->children.push_back(c);
		}

		b->children.push_back(makeDummyTree(numBranches - 1));

		
	}
	return b;
}

void Tree::renderTree() {
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glTranslatef(m_position.x, m_position.y, m_position.z);

	//Actually draw the tree
	renderBranch(root);

	// Clean up
	glPopMatrix();
}

void Tree::renderBranch(branch *b) {
	if(b == NULL){
		return;
	}
	if(windEnabled){
		applyWind(b);
	}
	glPushMatrix();

		//only draw branch info if it has a length
		if(b->length > 0){
		
			vec3 rot = b->basisRot;
			glRotatef(rot.z, 0, 0, 1);
			glRotatef(rot.y, 0, 1, 0);
			glRotatef(rot.x, 1, 0, 0);

			cout << b->name << endl;
			cout << "Branch Rotation X: " <<  b->rotation.x << endl;
			//cout << "Branch Rotation Y: " <<  b->rotation.y << endl;
			cout << "Branch Rotation Z: " <<  b->rotation.z << endl;
			cout << endl;

			glRotatef(b->rotation.z, 0, 0, 1);
			//glRotatef(b->rotation.y, 0, 1, 0);
			glRotatef(b->rotation.x, 1, 0, 0);
	
			glRotatef(-rot.x, 1, 0, 0);
			glRotatef(-rot.y, 0, 1, 0);
			glRotatef(-rot.z, 0, 0, 1);

			//glDisable(GL_LIGHTING);
			//draw the axes of this branch
			//drawAxis(b);

			//draw the joint of this branch
			drawJoint(b);

			//glEnable(GL_LIGHTING);

			//draw the branch itself
			drawBranch(b);

			vec3 dir = b->direction;
			//translate to the end of the branch based off length and direction
			glTranslatef(dir.x*b->length, dir.y*b->length, dir.z*b->length);

		}
		//loop through all child branches and render them too
		for(branch* child : b->children){
			renderBranch(child);
		}	
	glPopMatrix();
}

void Tree::drawJoint(branch* b){
	glPushMatrix();
		//colour cyan
		glColor3f(0,1,1);
		cgraSphere(b->baseWidth);
	glPopMatrix();
}

void Tree::drawBranch(branch* b){
	vec3 dir = b->direction;
	vec3 vec = vec3(0,0,1);
	vec3 norm = normalize(dir);
	float dotProd = dot(norm, vec);
	float angle = acos(dotProd);
	vec3 crossProd = cross(dir, vec);
	
	glPushMatrix();
	//colour grey
		glColor3f(1,1,1);
		glRotatef(-degrees(angle), crossProd.x, crossProd.y, crossProd.z);
		cgraCylinder(b->baseWidth, b->topWidth, b->length);
	glPopMatrix();
}

void Tree::drawAxis(branch* b){
	//X-axes
	glPushMatrix();
		glColor3f(1,0,0);
		glRotatef(90,0, 1,0);
		cgraCylinder(0.3*width, 0.3*width, 4.0*width);
		glTranslatef(0, 0, 4.0*width);
		cgraCone(1.2*width, 1.2*width);
	glPopMatrix();

	//Y-axes
	glPushMatrix();
		glColor3f(0,1,0);
		glRotatef(-90,1,0,0);
		cgraCylinder(0.3*width, 0.3*width, 4.0*width);
		glTranslatef(0,0, 4.0*width);
		cgraCone(1.2*width, 1.2*width);
	glPopMatrix();

	//Z-axes
	glPushMatrix();
		glColor3f(0,0,1);
		cgraCylinder(0.3*width, 0.3*width, 4.0*width);		
		glTranslatef(0,0,4.0*width);
		cgraCone(1.2*width, 1.2*width);	
	glPopMatrix();
}

void Tree::setPosition(vec3 position) {
	m_position = position;
}

void Tree::setWindForce(vec3 wind){
	windForce = wind;
}

float Tree::calculatePressure(branch* branch, float force){
	float t = branch->baseWidth - branch-> topWidth;

	float a = 1.0f; //change to a small number derived from the current angle of the branch
	//float b = math::random(0.0f,0.1f); //change to random value to make different branches be at different point of sine equation

	float oscillation = (time+branch->offset);

	//remove *degrees if calculation should be in radians
	float degrees =  (float)(math::pi());
	degrees = degrees / 180.0f;

	float pressure = force * (1 + a * sin(oscillation) );

	return pressure;
}

float Tree::springConstant(branch* branch){
	float k = (elasticity*branch->baseWidth*pow(branch->baseWidth-branch->topWidth, 3));
	k = k/ (4*pow(branch->length, 3));

	return k;
}

float Tree::displacement(branch* branch, float pressure){
	float spring = springConstant(branch);

	return pressure/spring;
}

void Tree::applyWind(branch* b){
	time += 0.000008f;

	float displacementX = displacement(b, calculatePressure(b, (windForce.x)));
	//float displacementY = displacement(b, calculatePressure(b, (windForce.y)));
	float displacementZ = displacement(b, calculatePressure(b, (windForce.z)));

	cout << "length " << b->length << endl;
	cout << "Displacement - x: " << displacementX /*<< "  y: " << displacementY */<< "  z: " << displacementZ << endl;
	
	float degrees =  ( (float)(math::pi()) ) / 180.0f;

	int len = b->length;

	if(len == 0){
		len = 0.00001f;
	}

	float motionAngleX = asin(displacementX/float(len)); //* degrees;
	//float motionAngleY = asin(displacementY/float(len)); //* degrees;
	float motionAngleZ = asin(displacementZ/float(len));// * degrees;

	cout << "Motion Angle - x: " << motionAngleX /*<< "  y: " << motionAngleY */<< "  z: " << motionAngleZ << endl;
	cout << asin(5) << endl;
	cout << endl;

	b->rotation.x = motionAngleX;
	//b->rotation.y = motionAngleY;
	b->rotation.z = motionAngleZ;

	b->rotation.x = displacementX;
	//b->rotation.y = displacementY;
	b->rotation.z = displacementZ;
}

void Tree::toggleWind(){
	windEnabled = ! windEnabled;
}
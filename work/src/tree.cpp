#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "cgra_geometry.hpp"
#include "fuzzy_object.hpp"
#include "cgra_math.hpp"
#include "opengl.hpp"
#include "tree.hpp"

using namespace std;
using namespace cgra;


Tree::Tree(float height, float trunk, float branchLength, float influenceRatio, float killRatio, float branchTipWidth, float branchMinWidth){
	treeHeight = height;
	trunkHeight = trunk;

	prm_branchLength = branchLength;
	prm_radiusOfInfluence = influenceRatio * prm_branchLength;
	prm_killDistance = killRatio * prm_branchLength;
	prm_branchTipWidth = branchTipWidth;
	prm_branchMinWidth = branchMinWidth;

	generateEnvelope(20);
	generateAttractionPointsVolumetric(300);

	generatedTreeRoot = generateTree();
	generateGeometry(generatedTreeRoot);
	dummyTreeRoot = makeDummyTree(4); // make dummy tree to work with
	setAccumulativeValues(generatedTreeRoot, 0, vec3(0,0,0));

	if(dummyTree){
		root = dummyTreeRoot;
	} else {
		root = generatedTreeRoot;
	}
}

Tree::~Tree() {
	for (branch* b : treeNodes) {
		delete(b);
	}

	for (FuzzyObject* fuzzySystem : fuzzyBranchSystems) {
		delete(fuzzySystem);
	}
}

branch* Tree::generateTree(){

	float d = prm_branchLength;

	branch *root = new branch();
	branch *parent = root;
	branch *curNode = root;
	curNode->position = vec3(0,0,0);
	curNode->direction = vec3(0,1,0);
	curNode->length = trunkHeight < d ? d : trunkHeight;
	treeNodes.push_back(curNode);

	//Generate branches from attraction points
	// int prevSize = attractionPoints.size() + 1;
	while(attractionPoints.size() > 0){
		// cout << "treeSize " << treeNodes.size() << " attPoints " << attractionPoints.size() << endl;

		vector<vector<int>> closestSet = getAssociatedPoints();
		vector<branch *> toBeAdded;
		//Loop for all treeNodes
		for(int t=0; t<treeNodes.size(); t++){
			//Check if we want to branch
			if(closestSet[t].size() > 0){
				vec3 v = treeNodes[t]->position + (treeNodes[t]->direction * treeNodes[t]->length);
				vec3 newDir = vec3(0,0,0);

				for(int j=0; j<closestSet[t].size(); j++){
					int ind = closestSet[t][j];
					newDir += normalize(attractionPoints[ind] - v);
				}
				newDir = normalize(newDir + vec3(0,-0.2,0));

				branch* newNode = new branch();
				newNode->position = v;
				newNode->direction = newDir;
				newNode->length = d;
				newNode->parent = treeNodes[t];
				newNode->offset = math::random(0.0f,1.0f);

				treeNodes[t]->children.push_back(newNode);

				toBeAdded.push_back(newNode);
			}
		}
		treeNodes.insert(treeNodes.end(), toBeAdded.begin(), toBeAdded.end());
		cullAttractionPoints();
		// prevSize = attractionPoints.size();
	}
	
	simplifyGeometry(root);
	setWidth(root);
	root->baseWidth = root->topWidth;
	return root;
}

float Tree::setWidth(branch *b){
	float width = 0.0;
	float maxW = prm_branchTipWidth;

	//cout << "branch with children: " << b->children.size() << endl;

	for(int i=0; i<b->children.size(); i++){
		float cw = setWidth(b->children[i]);
		width += pow(cw, 2);
		maxW = (cw > maxW) ? cw : maxW;
	}

	width = (width == 0) ? prm_branchMinWidth : sqrt(width);

	b->topWidth = maxW;
	b->baseWidth = width;

	return width;
}

void Tree::setAccumulativeValues(branch* b, int parents, vec3 totalRotation) {
	b->numParents = parents;
	b->combinedRotation += totalRotation;

	for (branch* c : b->children) {
		setAccumulativeValues(c, parents + 1, b->combinedRotation);
	}
}

void Tree::simplifyGeometry(branch *b){
	for(int i=0; i< b->children.size(); i++){
		branch *c1 = b->children[i];
		for(int j=0; j< b->children.size(); j++){
			if(i != j){
				branch *c2 = b->children[j];
				float angle = degrees(acos(dot(c1->direction, c2->direction)));
				if(angle < 5.0f && angle > -5.0f){
					//branches are very similar so combine them
					for (branch *cc : c2->children) {
						cc->parent = c1;
					}
					//Add children to the other branch
					c1->children.insert(c1->children.end(), c2->children.begin(), c2->children.end());
					//Remove child from parent
					branch* temp = b->children[b->children.size() - 1];
					b->children[b->children.size() - 1] = b->children[j];
					b->children[j] = temp;
					b->children.pop_back();
					j--;
				}
			}
		}
	}
	for (branch * c : b->children) {
		simplifyGeometry(c);
	}
}

void Tree::generateGeometry(branch *b) {
	b->jointModel = generateSphereGeometry(b->baseWidth);

	b->branchModel = generateCylinderGeometry(b->baseWidth, b->topWidth, b->length, 10, 2);

	b->jointModel->setMaterial(m_ambient, m_diffuse, m_specular, m_shininess, m_emission);
	b->branchModel->setMaterial(m_ambient, m_diffuse, m_specular, m_shininess, m_emission);

	b->branchFuzzySystem = new FuzzyObject(b->branchModel);

	float maxWidth = generatedTreeRoot->baseWidth;
	float minWidth = prm_branchTipWidth;

	float maxDensity = 1.2f;
	float minDensity = 0.5f;

	float amount = (b->baseWidth - minWidth) / (maxWidth - minWidth) * (maxDensity - minDensity) + minDensity;
	b->branchFuzzySystem->scaleDensity(amount);

	fuzzyBranchSystems.push_back(b->branchFuzzySystem);

	for (branch* c : b->children) {
		generateGeometry(c);
	}
}

//------------------------------------------------//
//   Attraction Point Functions                   //
//------------------------------------------------//

vector<vector<int>> Tree::getAssociatedPoints(){
	vector<vector<int>> closestNodes;
	//init all the sets;
	for(int j=0; j<treeNodes.size(); j++){
		vector<int> sv = vector<int>();
		closestNodes.push_back(sv);
	}
	//Scan through all attraction points
	for(int i=0; i<attractionPoints.size(); i++){
		vec3 aPoint = attractionPoints[i];

		float minDist = distance(aPoint,treeNodes[0]->position + (treeNodes[0]->direction * treeNodes[0]->length));
		int closest = 0;

		for(int j=1; j<treeNodes.size(); j++){
			branch* t = treeNodes[j];
			vec3 p = t->position + (t->direction * t->length);
			float dist = distance(aPoint,p);
			if(dist <= minDist){
				closest = j;
				minDist = dist;
			}
		}

		//Only assign to the set if it is within the radius of influence
		if(minDist <= prm_radiusOfInfluence){
			closestNodes[closest].push_back(i);
		}
	}
	return closestNodes;
}

void Tree::cullAttractionPoints(){
	int countRemoved = 0;

	for(int i=0; i<attractionPoints.size() - countRemoved;){
		vec3 aPoint = attractionPoints[i];

		bool toRemove = false;
		for(int j=0; j<treeNodes.size(); j++){
			branch* t = treeNodes[j];
			vec3 p = t->position + (t->direction * t->length);
			if(distance(aPoint,p) < prm_killDistance){
				toRemove = true;
				break;
			}
		}
		if(toRemove){
			vec3 temp = attractionPoints[i];
			int ind = attractionPoints.size() - (1+countRemoved);
			attractionPoints[i] = attractionPoints[ind];
			attractionPoints[ind] = temp;

			countRemoved++;
		}else{
			i++;
		}
	}
	if(attractionPoints.size() <= countRemoved){
		attractionPoints.erase(attractionPoints.begin(), attractionPoints.end());
	}else{
		attractionPoints.erase(attractionPoints.end() - (1+countRemoved),attractionPoints.end());
	}
}

void Tree::generateAttractionPoints(int numPoints){
	if(numPoints == 0) return;

	vector<vec3> points;
	while(points.size() < numPoints){
		//Calculate random height/rotation
		float y = math::random(trunkHeight,treeHeight);
		float theta = math::random(0.0f,360.0f);

		// Calculate max distance
		float d = envelopeFunction(y,theta);

		// Calculate distance away from central axis
		float r = math::random(0.0f,d);

		// Convert from rotation/distance to x,z
		points.push_back(vec3(r * sin(radians(theta)), y, r * cos(radians(theta))));
	}
	attractionPoints = points;
}

void Tree::generateAttractionPointsVolumetric(int numPoints){
	if(numPoints == 0) return;

	vector<vec3> points;
	while(points.size() < numPoints){
		float x = math::random(minX,maxX);
		float y = math::random(trunkHeight,treeHeight);
		float z = math::random(minZ,maxZ);

		vec3 point = vec3(x,y,z);

		if(inEnvelope(point)){
			points.push_back(point);
		}
	}
	attractionPoints = points;
}



//------------------------------------------------//
//   Envelope Functions                           //
//------------------------------------------------//
void Tree::generateEnvelope(int steps){
	vector<vector<vec3>> env;

	yStep = (treeHeight - trunkHeight)/steps;
	float y;

	for(int i = 0; i <= steps; i++){
		vector<vec3> layer;
		y = (i * yStep) + trunkHeight;
		for(float theta = 0; theta <= 360.0f; theta += thetaStep){
			float d = envelopeFunction(y-trunkHeight,theta);

			float x = d * sin(radians(theta));
			float z = d * cos(radians(theta));

			//Assign bounding values for volumetric filling
			minZ = z < minZ ? z : minZ;
			maxZ = z > maxZ ? z : maxZ;
			minX = z < minX ? z : minX;
			maxX = z > maxX ? z : maxX;

			layer.push_back(vec3(x, y, z));
		}
		env.push_back(layer);
	}
	envelope = env;
}

bool Tree::inEnvelope(vec3 point){
	float x = point.x;
	float y = point.y;
	float z = point.z;
	//Make sure is within y bounds of envelope
	if(y < trunkHeight || y > treeHeight){
		return false;
	}
	//Calculate the level of the point;
	int yInd1 = int((y - trunkHeight)/yStep);
	int yInd2 = yInd1 + 1;
	// Ratio between y of points at yInd1/2
	float deltaY = (y - ((yInd1 * yStep) + trunkHeight))/yStep;

	vector<vec3> layer1 = envelope[yInd1];
	vector<vec3> layer2 = envelope[yInd2];

	float radius = distance(vec3(0,y,0), point);
	float theta = degrees(asin(x/radius));
	theta = theta < 0.0f ? 360.0f + theta : theta;

	// Calculate indicies based off rotation
	int xzInd1 = int(theta/thetaStep);
	int xzInd2 = xzInd1 + 1;
	// Ratio between the rotations of points at xzInd1/2
	float deltaT = (theta - (xzInd1 * thetaStep))/thetaStep;

	// Calculate the points between the two points with the y ratio
	vec3 xzP1 = layer1[xzInd1] + (deltaY * (layer2[xzInd1] - layer1[xzInd1]));
	vec3 xzP2 = layer1[xzInd2] + (deltaY * (layer2[xzInd2] - layer1[xzInd2]));

	// Calculate the point between the two points with the rotation ratio
	vec3 maxP = xzP1 + (deltaT * (xzP2 - xzP1));
	float maxRadius = distance(vec3(0,y,0), maxP);

	return radius <= maxRadius;
}

float Tree::envelopeFunction(float u, float theta){
	float uN = u/(treeHeight-trunkHeight);
	// return 6*(pow(3,2*uN) - (8*uN*uN*uN));
	// return (1.0f - uN) * 8;
	 return -100 * (uN * uN * (uN - 1));
	// return 6;
}

//------------------------------------------------//
//   Rendering Functions                          //
//------------------------------------------------//

void Tree::drawEnvelope(){
	for(int i=0; i<envelope.size(); i++){
		vector<vec3> layer = envelope[i];
		
		for(int j=0; j<layer.size()-1; j++){
			vec3 p = layer[j];
			vec3 p1 = layer[j+1];
			vec3 q = (i < envelope.size() - 1) ? envelope[i+1][j] : layer[j+1];


			glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, vec4(i / float(envelope.size()), 0.5f, 0.0f, 1.0f).dataPointer());

			glBegin(GL_LINES);
			glVertex3f(p.x, p.y, p.z);
			glVertex3f(p1.x, p1.y, p1.z);

			glVertex3f(p.x, p.y, p.z);
			glVertex3f(q.x, q.y, q.z);
			glEnd();
		}
	}
}

void Tree::renderAttractionPoints(){
	for(int i=0; i< attractionPoints.size(); i++){
		glPushMatrix();
		vec3 p = attractionPoints[i];
		glTranslatef(p.x,p.y,p.z);
		cgraSphere(0.1);
		glPopMatrix();
	}
}

/* public method for drawing the tree to the screen.
	draws the tree by calling renderbranch() on the root node.
*/
void Tree::renderTree(bool wireframe) {
	//glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	//makes sure the tree is drawn at its set position
	glTranslatef(m_position.x, m_position.y, m_position.z);

	//Actually draw the tree

	setAccumulativeValues(root, 0, vec3(0,0,0));
	updateWorldWindDirection(root, vec3(0,0,0));
	renderBranch(root, wireframe);

	//increment wind "time"
	time += timeIncrement;

	// Clean up
	glPopMatrix();
}

/* performs the logic for drawing any given branch at its position and rotation.
	then recursively calls renderBranch() on all of its child branches.
*/
void Tree::renderBranch(branch *b, bool wireframe, int depth) {
	if(b == NULL){
		return;
	}
	//togglable for starting and stopping the wind being applied
	if(windEnabled){
		applyWind(b);
	}

	static int i_k =0;

	glPushMatrix();
		//only draw branch info if it has a length
		if(b->length > 0){
			//perform rotation as updated by wind
			glRotatef(b->rotation.z, 0, 0, 1);
			glRotatef(b->rotation.x, 1, 0, 0);

			//draw the joint of this branch
			drawJoint(b, wireframe);

			drawBranch(b, wireframe);

			//translate to the end of the branch based off length and direction
			vec3 offset = b->direction * b->length;
			glTranslatef(offset.x,offset.y,offset.z);
		}

		//loop through all child branches and render them too
		for(branch* c : b->children){
			renderBranch(c, wireframe, depth+1);
		}

	glPopMatrix();
}


/* draws a joint at the base of every branch the size of the width at the base of the branch
	this prevents a tree breaking visual issue when rotating branches.
*/
void Tree::drawJoint(branch* b, bool wireframe){
	if (!wireframe && !fuzzySystemFinishedBuilding) {
		glPushMatrix();
			b->jointModel->renderGeometry(wireframe);
		glPopMatrix();
	}
}

/* draws the branch to the screen
*/
void Tree::drawBranch(branch* b, bool wireframe){
	vec3 norm = normalize(b->direction);
	float dotProd = dot(norm, vec3(0,0,1));

	float angle = acos(dotProd); // the angle to rotate by
	vec3 crossProd = cross(b->direction, vec3(0,0,1));

	glPushMatrix();
		glRotatef(-degrees(angle), crossProd.x, crossProd.y, crossProd.z);
		if (!fuzzySystemFinishedBuilding){
			b->branchModel->renderGeometry(wireframe);
			if((b->baseWidth < 2 * prm_branchMinWidth) && !wireframe){
				//drawLeaves(b,leaves);
			}
		}
		b->branchFuzzySystem->renderSystem();
	glPopMatrix();
}

void Tree::drawLeaves(branch* b){
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin(GL_QUADS);
	
	float l = ((b->children.size() < 1) ? 2.0 : 1.0) * b->length;
	float w = 0.17f * l;

	glNormal3f(0,1,0);

	glTexCoord2f(0,1);
	glVertex3f(-w,0,0);

	glTexCoord2f(1,1);
	glVertex3f(w,0,0);

	glTexCoord2f(1,0);
	glVertex3f(w,0,l);

	glTexCoord2f(0,0);
	glVertex3f(-w,0,l);
	
	glEnd();
}

void Tree::renderStick(){
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	// glTranslatef(m_position.x, m_position.y, m_position.z);

	//Actually draw the skeleton
	renderStick(root);

	// Clean up
	glPopMatrix();
}

void Tree::renderStick(branch *b, int depth){
	glPushMatrix();{
		glBegin(GL_LINES);
		vec3 p1 = b->position;
		vec3 p2 = b->position + (b->direction * b->length);
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, vec4(p1, 1.0f).dataPointer());
		glVertex3f(p1.x,p1.y,p1.z);
		glVertex3f(p2.x,p2.y,p2.z);
		glEnd();

		// vec3 dir = b->direction;
		// vec3 offset = dir * b->length;
		// glBegin(GL_LINES);
		// 	glVertex3f(0.0f,0.0f,0.0f);
		// 	glVertex3f(offset.x,offset.y,offset.z);
		// glEnd();
		// glTranslatef(offset.x,offset.y,offset.z);

		for(branch* child : b->children){
			renderStick(child, depth+1);
		}
	}glPopMatrix();
}


void Tree::updateWorldWindDirection(branch* b, vec3 previousVector){	
	if(b == NULL){
		return;
	}

	b->worldDir = previousVector;

	vec3 currentVector = b->direction * b->length;
	vec3 total = currentVector + previousVector;

	for(branch* c : b->children){
		updateWorldWindDirection(c, total);
	}
}

/*
	Calculates the pressure the wind will apply to a given branch
	force is the float value of the wind in the windforce vector for a given axis (x or z)
	dir is an int to let us know which axis to calculate the force for (0 == x, 2 == z)
*/
float Tree::calculatePressure(branch* b, float force, int dir){
	float a = windCoefficent; //change to a small number derived from the current angle of the branch

	//attempt at making the small value use the current angle of the branch
	// if (dir == 'x'){ //x axis
	// 	a = sin(branch->rotation.z);
	// } else if (dir == 'z'){ //z axis
	// 	a = sin(branch->rotation.x);
	// }
	float dotProd = dot(b->worldDir, desiredWindForce);
	float angle = acos(dotProd); // the angle to rotate by

	//force = sin(angle);

	//oscillation is plugged into a sine function.
	//time is increased steadily to make the effect follow an oscilation pattern - global scope
	//branch offset is a random value assigned to each branch so they are at a different point in the oscillation
	float oscillation = (time + b->offset);
	//oscillation = (oscillation - floor(oscillation)) - 0.5f;

	//mulitply a radians value by degrees variable to convert it from radians to degrees
	//not sure if this is needed...
	float degrees = 180.0f / ((float)math::pi()) ;

	//pressure is the final return value
	float pressure = force * (1 + (angle*2) * sin(oscillation) );
	// float pressure = force + ((force*angle) * (force*sin(oscillation)));
	// float pressure = angle  + sin(oscillation);
	// float pressure = sin(oscillation);

	return pressure;
}


/*
	A spring value for a branch based on its thickness and length.
	Taken from a reserch paper
*/
float Tree::springConstant(branch* branch){
	float thickness = (branch->baseWidth+branch->topWidth)/2.0f;

	float k = (elasticity * branch->baseWidth *	pow(thickness, 2));

	// cout << "Spring top: " << k << endl;
	// cout << "Spring bot: " << (4 * pow( branch->length, 3)) << endl;

	k = k / (4 * pow( branch->length, 3));

	return k;
}

/*
	the central method for applying wind force to a branch.
	calculates the displacement value for the branch based on the wind then
	stores the value to rotate it by
*/
void Tree::applyWind(branch* b){
	//increment time (this value is currently has no meaning, just seems to fit at an ok speed)

	//calculates the pressure value for each axis
	float pressureX = calculatePressure(b, desiredWindForce.x, 'x');
	float pressureZ = calculatePressure(b, desiredWindForce.z, 'z');

	//debug info
	// cout << "Name: " << b->name << endl;
	// cout << "Pressure X: " << pressureX << endl;
	// cout << "Pressure Z: " << pressureZ << endl;

	//the spring value of this branch
	float spring = springConstant(b);
	// cout << "Spring Value: " << spring << endl;

	//make sure no division of 0 is occuring
	int len = b->length;
	if(len == 0){
		len = 0.00001f;
	}
	if(spring == 0){
		spring = 0.00001f;
	}

	//calculates the displacement value for each axis
	float displacementX = pressureX / spring / float(len);
	float displacementZ = pressureZ / spring / float(len);

	//debug info
	// cout << "length " << b->length << endl;
	// cout << "Displacement - x: " << displacementX << "  z: " << displacementZ << endl;

	//clamp values to be within [-1,1]
	if (displacementX > 1){
		displacementX = 1.0f;
	} else if (displacementX < -1){
		displacementX = -1.0f;
	}

	if (displacementZ > 1){
		displacementZ = 1.0f;
	} else if (displacementZ < -1){
		displacementZ = -1.0f;
	}

	float motionAngleX = asin(displacementX);
	float motionAngleZ = asin(displacementZ);

	// cout << "Motion Angle - x: " << motionAngleX << "  z: " << motionAngleZ << endl;

	//mulitply a radians value by degrees variable to convert it from radians to degrees
	float degrees = 180.0f / ((float)math::pi());
	b->rotation.x = motionAngleX * degrees;
	b->rotation.z = motionAngleZ * degrees;


	if(motionAngleX > b->maxX){
		b->maxX = motionAngleX;
	} else if (motionAngleX < b->minX){
		b->minX = motionAngleX;
	}
	if(motionAngleZ > b->maxZ){
		b->maxZ = motionAngleZ;
	} else if (motionAngleZ < b->minZ){
		b->minZ = motionAngleZ;
	}



	float clampAngle = 10.0f;
	// b->rotation.x = (b->rotation.x - b->minX) / (-clampAngle - b->minX) * (clampAngle - maxX) + maxX;
	// b->rotation.z = (b->rotation.z - b->minZ) / (-clampAngle - b->minZ) * (clampAngle - maxZ) + maxZ;
	
	// cout << "Rotation Angle - x: " << b->rotation.x << "  z: " << b->rotation.z << endl;

	//public static float Remap (this float value, float from1, float to1, float from2, float to2) {
    //return (value - from1) / (to1 - from1) * (to2 - from2) + from2;


	// if (b->rotation.z > clampAngle){
	// 	b->rotation.z = clampAngle;
	// } else if (b->rotation.z < -clampAngle){
	// 	b->rotation.z = -clampAngle;
	// }
	// if (b->rotation.x > clampAngle){
	// 	b->rotation.x = clampAngle;
	// } else if (b->rotation.x < -clampAngle){
	// 	b->rotation.x = -clampAngle;
	// }

	if (b->combinedRotation.x > clampAngle){
		b->rotation.x = -b->rotation.x;
	} else if (b->combinedRotation.x < -clampAngle){
		b->rotation.x = -b->rotation.x;
	} 

	if (b->combinedRotation.z > clampAngle){
		b->rotation.z = -b->rotation.z;
	} else if (b->combinedRotation.z < -clampAngle){
		b->rotation.z = -b->rotation.z;
	} 

}

//------------------------------------------------//
//   Miscellaneous Functions                      //
//------------------------------------------------//
/*
	sets the position we want to draw the tree at
*/
void Tree::setPosition(vec3 position) {
	m_position = position;
}

void Tree::setMaterial(vec4 ambient, vec4 diffuse, vec4 specular, float shininess, vec4 emission){
	m_ambient = ambient;
	m_diffuse = diffuse;
	m_specular = specular;
	m_shininess = shininess;
	m_emission = emission;
}

/*
	public method for toggling wind off and on
*/
void Tree::toggleWind(){
	windEnabled = ! windEnabled;
}

/*
	public method for toggling between dummy tree model and randomly generated tree
*/
void Tree::toggleTreeType(){
	dummyTree = ! dummyTree;
	if(dummyTree){
		root = dummyTreeRoot;
	} else {
		root = generatedTreeRoot;
	}
}

/*
	sets the wind force
*/
void Tree::setWindForce(vec3 wind){
	desiredWindForce = wind;
}

void Tree::adjustWind(int axis, int dir){
	float wIncrease = 0.00005f;
	float aIncrease = 0.1f;
	float tIncrease = 0.002f;

	if (axis == 'x'){
		if (dir == 1){
			desiredWindForce.x += wIncrease;
		} else if (dir == -1){
			desiredWindForce.x -= wIncrease;
		}
	} else if (axis == 'z'){
		if (dir == 1){
			desiredWindForce.z += wIncrease;
		} else if (dir == -1){
			desiredWindForce.z -= wIncrease;
		}
	}  else if (axis == 'a'){
		if (dir == 1){
			windCoefficent += aIncrease;
		} else if (dir == -1){
			windCoefficent -= aIncrease;
		}
	}  else if (axis == 't'){
		if (dir == 1){
			timeIncrement += tIncrease;
		} else if (dir == -1){
			timeIncrement -= tIncrease;
		}
	}
}

vec3 Tree::getWindForce() {
	return desiredWindForce;
}

void Tree::buildFuzzySystems(bool increment) {
	for (FuzzyObject* fuzzySystem : fuzzyBranchSystems) {
		fuzzySystem->buildSystem(increment);
	}

	// Check if the fuzzy systems have finished building
	for (FuzzyObject* fuzzySystem : fuzzyBranchSystems) {
		if (!fuzzySystem->finishedBuilding()) {
			return;
		}
	}
	fuzzySystemFinishedBuilding = true;
}

bool Tree::finishedBuildingFuzzySystems() {
	return fuzzySystemFinishedBuilding;
}

vector<vec3> Tree::getFuzzySystemPoints() {
	vector<vec3> points;

	getBranchFuzzySystemPoints(root, &points);

	// Clear the fuzzy systems as they are done
	for (FuzzyObject* fuzzySystem : fuzzyBranchSystems) {
		fuzzySystem->clearParticles();
	}

	return points;
}

int Tree::getFuzzySystemParticleCount() {
	int total = 0;

	for (FuzzyObject* fuzzySystem : fuzzyBranchSystems) {
		total += fuzzySystem->getParticleCount();
	}

	return total;
}

void Tree::getBranchFuzzySystemPoints(branch* b, vector<vec3>* points) {
	vector<vec3> systemPoints = b->branchFuzzySystem->getSystem();

	for (int i = 0; i < systemPoints.size(); i++) {

		// Create the vector that will contain the baked particle position
		vec3 bakedPosition = systemPoints[i];

		// Rotate the vector by the direction vector
		vec3 axis = cross(b->direction, vec3(0, 0, 1));
		float dotProd = dot(b->direction, vec3(0, 0, 1));
		float acosAngle = acos(dotProd);
		bakedPosition = bakedPosition * angleAxisRotation(acosAngle, axis);

		// mat4 mat;
		// bakedPosition = vec3(vec4(bakedPosition,1.0f) * mat.rotateZ(b->rotation.z));
		// bakedPosition = vec3(vec4(bakedPosition,1.0f) * mat.rotateX(b->rotation.x));

		// Translate the vector
		bakedPosition += b->worldDir;

		points->push_back(vec3(bakedPosition.x, bakedPosition.y, bakedPosition.z));
	}


	for (branch* c : b->children) {
		getBranchFuzzySystemPoints(c, points);
	}
}

mat3 Tree::angleAxisRotation(float angle, vec3 u) {
	mat3 m;

	m[0][0] = cos(angle) + u.x * u.x * (1 - cos(angle));
	m[0][1] = u.y * u.x * (1 - cos(angle)) + u.z * sin(angle);
	m[0][2] = u.z * u.x * (1 - cos(angle)) - u.y * sin(angle);

	m[1][0] = u.x * u.y * (1 - cos(angle)) - u.z * sin(angle);
	m[1][1] = cos(angle) + u.y * u.y * (1 - cos(angle));
	m[1][2] = u.z * u.y * (1 - cos(angle)) + u.x * sin(angle);

	m[2][0] = u.x * u.z * (1 - cos(angle)) + u.y * sin(angle);
	m[2][1] = u.y * u.z * (1 - cos(angle)) - u.x * sin(angle);
	m[2][2] = cos(angle) + u.z * u.z * (1 - cos(angle));

	return m;
}

/* Builds a test tree to work with for simulating wind animation.
	Trees is 'numBranches' segments tall, with 4 branches inbetween each segment.
*/
branch* Tree::makeDummyTree(int numBranches){
	//hardcoded values for this dummy tree
	float width = 0.1f;
	float length = 5.0f;

	branch* b = new branch();
	b->name = "trunk"+to_string(numBranches);
	b->direction = vec3(0,1,0);
	b->offset = math::random(0.0f,1.0f);
	b->length = length;
	b->baseWidth = width * numBranches;
	b->topWidth = (width * (numBranches - 1));
	if(numBranches == 1){
		b->topWidth = 0.0001f;//(width /2);
	}

	b->basisRot = vec3(0,0,0);
	if(numBranches > 1){

		for (int i = 0; i < 4; i++){
			branch* c = new branch();

			c->name = "branch"+to_string(i+1)+" trunk"+to_string(numBranches);

			if(i == 0){
				c->direction = vec3(1,0.3,0);
			}else if(i == 1){
				c->direction = vec3(-1,0.3,0);
			}else if(i == 2){
				c->direction = vec3(0,0.3,1);
			}else if(i == 3){
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

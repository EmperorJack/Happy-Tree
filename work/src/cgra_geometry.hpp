#pragma once

#include "cgra_math.hpp"
#include "opengl.hpp"
#include "geometry.hpp"

namespace cgra {

	inline void cgraSphere(float radius, int slices = 10, int stacks = 10, bool wire = false) {
		assert(slices > 0 && stacks > 0 && radius > 0);

		// set wire mode if needed
		if (wire) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		int dualslices = slices * 2;

		// precompute sin/cos values for the range of phi
		std::vector<float> sin_phi_vector;
		std::vector<float> cos_phi_vector;

		for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
			float phi = 2 * math::pi() * float(slice_count) / dualslices;
			sin_phi_vector.push_back(std::sin(phi));
			cos_phi_vector.push_back(std::cos(phi));
		}


		// precompute the normalized coordinates of sphere
		std::vector<vec3> verts;

		for (int stack_count = 0; stack_count <= stacks; ++stack_count) {
			float theta = math::pi() * float(stack_count) / stacks;
			float sin_theta = std::sin(theta);
			float cos_theta = std::cos(theta);

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {


				verts.push_back(vec3(
					sin_theta*cos_phi_vector[slice_count],
					sin_theta*sin_phi_vector[slice_count],
					cos_theta));
			}
		}

		// use triangle strips to display each stack of the sphere
		for (int stack_count = 0; stack_count < stacks; ++stack_count) {
			glBegin(GL_TRIANGLE_STRIP);

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {

				vec3 &h = verts[slice_count + stack_count*(dualslices + 1)];
				vec3 &l = verts[slice_count + (stack_count + 1)*(dualslices + 1)];

				vec3 ph = h*radius;
				vec3 pl = l*radius;

				float uh = slice_count / float(dualslices);
				float vh = stack_count / float(stacks);
				float vl = (stack_count+1) / float(stacks);

				glNormal3f(h.x, h.y, h.z);
				glTexCoord2f(vh, uh);
				glVertex3f(ph.x, ph.y, ph.z);

				glNormal3f(l.x, l.y, l.z);
				glTexCoord2f(vl, uh);
				glVertex3f(pl.x, pl.y, pl.z);
			}

			glEnd();
		}

		// reset mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	inline void cgraCylinder(float base_radius, float top_radius, float height, int slices = 10, int stacks = 10, bool wire = false) {
		assert(slices > 0 && stacks > 0 && (base_radius > 0 || base_radius > 0) && height > 0);

		// set wire mode if needed
		if (wire) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		int dualslices = slices * 2;

		// precompute sin/cos values for the range of phi
		std::vector<float> sin_phi_vector;
		std::vector<float> cos_phi_vector;

		for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
			float phi = 2 * math::pi() * float(slice_count) / dualslices;
			sin_phi_vector.push_back(std::sin(phi));
			cos_phi_vector.push_back(std::cos(phi));
		}


		// precompute the coordinates and normals of cylinder
		std::vector<vec3> verts;
		std::vector<vec3> norms;

		// thanks ben, you shall forever be immortalized
		float bens_theta = math::pi() / 2 * std::atan((base_radius - top_radius) / height);
		float sin_bens_theta = std::sin(bens_theta);
		float cos_bens_theta = std::cos(bens_theta);

		for (int stack_count = 0; stack_count <= stacks; ++stack_count) {
			float t = float(stack_count) / stacks;
			float z = height * t;
			float width = base_radius + (top_radius - base_radius) * t;

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
				verts.push_back(vec3(
					width * cos_phi_vector[slice_count],
					width * sin_phi_vector[slice_count],
					z));

				norms.push_back(vec3(
					cos_bens_theta * cos_phi_vector[slice_count],
					cos_bens_theta * sin_phi_vector[slice_count],
					sin_bens_theta));

			}
		}

		// use triangle strips to display each stack of the cylinder
		for (int stack_count = 0; stack_count < stacks; ++stack_count) {
			glBegin(GL_TRIANGLE_STRIP);

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {

				vec3 &ph = verts[slice_count + stack_count*(dualslices + 1)];
				vec3 &pl = verts[slice_count + (stack_count + 1)*(dualslices + 1)];

				vec3 &nh = norms[slice_count + stack_count*(dualslices + 1)];
				vec3 &nl = norms[slice_count + (stack_count + 1)*(dualslices + 1)];

				float sh = stack_count / float(stacks);
				float th = slice_count / float(slices);
				float t1 = (slice_count+1) / float(slices);

				glNormal3f(nh.x, nh.y, nh.z);
				glVertex3f(ph.x, ph.y, ph.z);
				glTexCoord2f(th, sh);

				glNormal3f(nl.x, nl.y, nl.z);
				glVertex3f(pl.x, pl.y, pl.z);
				glTexCoord2f(t1, sh);
			}

			glEnd();
		}

		// cap off the top and bottom of the cylinder
		if (base_radius > 0) {
			glBegin(GL_TRIANGLE_FAN);
			glNormal3f(0, 0, -1);
			glVertex3f(0, 0, 0);

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
				vec3 &p = verts[slice_count];
				glVertex3f(p.x, p.y, p.z);
			}
			glEnd();
		}

		if (top_radius > 0) {
			glBegin(GL_TRIANGLE_FAN);
			glNormal3f(0, 0, 1);
			glVertex3f(0, 0, height);

			for (int slice_count = dualslices; slice_count >= 0; --slice_count) {
				vec3 &p = verts[slice_count + (stacks)*(dualslices + 1)];
				glVertex3f(p.x, p.y, p.z);
			}
			glEnd();
		}

		// reset mode
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	inline void cgraCone(float base_radius, float height, int slices = 10, int stacks = 10, bool wire = false) {
		cgraCylinder(base_radius, 0, height, slices, stacks, wire);
	}

	// Pushes a new triangle into the given vector based on the given indices
	inline void createTriangle(std::vector<triangle>* triangles, int index0, int index1, int index2) {
		triangle tri;

		vertex v0;
		v0.p = index0;
		v0.n = index0;
		v0.t = index0;
		tri.v[0] = v0;

		vertex v1;
		v1.p = index1;
		v1.n = index1;
		v1.t = index1;
		tri.v[1] = v1;

		vertex v2;
		v2.p = index2;
		v2.n = index2;
		v2.t = index2;
		tri.v[2] = v2;
		triangles->push_back(tri);
	}

	// Generates a geometry object that contains mesh data for a cgra sphere
	inline Geometry* generateSphereGeometry(float radius, int slices = 10, int stacks = 10) {
		assert(slices > 0 && stacks > 0 && radius > 0);

		int dualslices = slices * 2;

		// precompute sin/cos values for the range of phi
		std::vector<float> sin_phi_vector;
		std::vector<float> cos_phi_vector;

		for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
			float phi = 2 * math::pi() * float(slice_count) / dualslices;
			sin_phi_vector.push_back(std::sin(phi));
			cos_phi_vector.push_back(std::cos(phi));
		}

		// precompute the normalized coordinates of sphere
		std::vector<vec3> verts;

		// The vectors that will make up the geometry object
		std::vector<vec3> points;
		std::vector<vec3> normals;
		std::vector<vec2> uvs;
		std::vector<triangle> triangles;

		// Load dummy points
		points.push_back(vec3(0,0,0));
		normals.push_back(vec3(0,0,1));
		uvs.push_back(vec2(0,0));

		// Counting fields
		int totalPointCount = 0;
		int pointCount = 0;

		for (int stack_count = 0; stack_count <= stacks; ++stack_count) {
			float theta = math::pi() * float(stack_count) / stacks;
			float sin_theta = std::sin(theta);
			float cos_theta = std::cos(theta);

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {

				verts.push_back(vec3(
					sin_theta*cos_phi_vector[slice_count],
					sin_theta*sin_phi_vector[slice_count],
					cos_theta));
			}
		}

		// use triangle strips to display each stack of the sphere
		for (int stack_count = 0; stack_count < stacks; ++stack_count) {

			totalPointCount += pointCount;
			pointCount = 0;

			// This section is a triangle strip

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {

				vec3 &h = verts[slice_count + stack_count*(dualslices + 1)];
				vec3 &l = verts[slice_count + (stack_count + 1)*(dualslices + 1)];

				vec3 ph = h*radius;
				vec3 pl = l*radius;

				float uh = slice_count / float(dualslices);
				float vh = stack_count / float(stacks);
				float vl = (stack_count+1) / float(stacks);

				normals.push_back(vec3(h.x, h.y, h.z)); //glNormal3f(h.x, h.y, h.z);
				points.push_back(vec3(ph.x, ph.y, ph.z)); //glVertex3f(ph.x, ph.y, ph.z);
				uvs.push_back(vec2(vh,uh));

				pointCount++;
				if (pointCount >= 3) {
					createTriangle(&triangles, totalPointCount + pointCount - 2, totalPointCount + pointCount - 1, totalPointCount + pointCount);
				}

				normals.push_back(vec3(l.x, l.y, l.z)); //glNormal3f(l.x, l.y, l.z);
				points.push_back(vec3(pl.x, pl.y, pl.z)); //glVertex3f(pl.x, pl.y, pl.z);
				uvs.push_back(vec2(vl,uh));

				pointCount++;
				if (pointCount >= 3 && stack_count != stacks - 1) {
					createTriangle(&triangles, totalPointCount + pointCount - 1, totalPointCount + pointCount - 2, totalPointCount + pointCount);
				}
			}
		}

		return new Geometry(points, normals, uvs, triangles, false);
	}

	// Generates a geometry object that contains mesh data for a cgra cylinder
	inline Geometry* generateCylinderGeometry(float base_radius, float top_radius, float height, int slices = 10, int stacks = 10) {
		assert(slices > 0 && stacks > 0 && (base_radius > 0 || base_radius > 0) && height > 0);

		int dualslices = slices * 2;

		// precompute sin/cos values for the range of phi
		std::vector<float> sin_phi_vector;
		std::vector<float> cos_phi_vector;

		for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
			float phi = 2 * math::pi() * float(slice_count) / dualslices;
			sin_phi_vector.push_back(std::sin(phi));
			cos_phi_vector.push_back(std::cos(phi));
		}

		// precompute the coordinates and normals of cylinder
		std::vector<vec3> verts;
		std::vector<vec3> norms;

		// The vectors that will make up the geometry object
		std::vector<vec3> points;
		std::vector<vec3> normals;
		std::vector<vec2> uvs;
		std::vector<triangle> triangles;

		// Load dummy points
		points.push_back(vec3(0,0,0));
		normals.push_back(vec3(0,0,1));
		uvs.push_back(vec2(0,0));

		// Counting fields
		int totalPointCount = 0;
		int pointCount = 0;

		// thanks ben, you shall forever be immortalized
		float bens_theta = math::pi() / 2 * std::atan((base_radius - top_radius) / height);
		float sin_bens_theta = std::sin(bens_theta);
		float cos_bens_theta = std::cos(bens_theta);

		for (int stack_count = 0; stack_count <= stacks; ++stack_count) {
			float t = float(stack_count) / stacks;
			float z = height * t;
			float width = base_radius + (top_radius - base_radius) * t;

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
				verts.push_back(vec3(
					width * cos_phi_vector[slice_count],
					width * sin_phi_vector[slice_count],
					z));

				norms.push_back(vec3(
					cos_bens_theta * cos_phi_vector[slice_count],
					cos_bens_theta * sin_phi_vector[slice_count],
					sin_bens_theta));
			}
		}

		// use triangle strips to display each stack of the cylinder
		for (int stack_count = 0; stack_count < stacks; ++stack_count) {

			totalPointCount += pointCount;
			pointCount = 0;

			// This section is a triangle strip

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {

				vec3 &ph = verts[slice_count + stack_count*(dualslices + 1)];
				vec3 &pl = verts[slice_count + (stack_count + 1)*(dualslices + 1)];

				vec3 &nh = norms[slice_count + stack_count*(dualslices + 1)];
				vec3 &nl = norms[slice_count + (stack_count + 1)*(dualslices + 1)];

				float uh = slice_count / float(dualslices);
				float vh = stack_count / float(stacks);
				float vl = (stack_count+1) / float(stacks);

				normals.push_back(vec3(nh.x, nh.y, nh.z)); //glNormal3f(nh.x, nh.y, nh.z);
				points.push_back(vec3(ph.x, ph.y, ph.z)); //glVertex3f(ph.x, ph.y, ph.z);
				uvs.push_back(vec2(uh,vh));

				pointCount++;
				if (pointCount >= 3) {
					createTriangle(&triangles, totalPointCount + pointCount - 1, totalPointCount + pointCount - 2, totalPointCount + pointCount);
				}

				normals.push_back(vec3(nl.x, nl.y, nl.z)); //glNormal3f(nl.x, nl.y, nl.z);
				points.push_back(vec3(pl.x, pl.y, pl.z)); //glVertex3f(pl.x, pl.y, pl.z);
				uvs.push_back(vec2(uh,vl));

				pointCount++;
				if (pointCount >= 3) {
					createTriangle(&triangles, totalPointCount + pointCount - 2, totalPointCount + pointCount - 1, totalPointCount + pointCount);
				}
			}
		}

		totalPointCount += pointCount;
		pointCount = 0;

		// cap off the top and bottom of the cylinder
		if (base_radius > 0) {

			// This section is using a triangle fan

			normals.push_back(vec3(0, 0, -1)); //glNormal3f(0, 0, -1);
			points.push_back(vec3(0, 0, 0)); //glVertex3f(0, 0, 0);

			pointCount++;
			int fanPointIndex = totalPointCount + pointCount;

			for (int slice_count = 0; slice_count <= dualslices; ++slice_count) {
				vec3 &p = verts[slice_count];
				points.push_back(vec3(p.x, p.y, p.z)); //glVertex3f(p.x, p.y, p.z);

				pointCount++;
				if (pointCount >= 3) {
					createTriangle(&triangles, totalPointCount + pointCount - 1, fanPointIndex, totalPointCount + pointCount);
				}
			}
		}

		totalPointCount += pointCount;
		pointCount = 0;

		if (top_radius > 0) {

			// This section is using a triangle fan

			normals.push_back(vec3(0, 0, 1)); //glNormal3f(0, 0, 1);
			points.push_back(vec3(0, 0, height)); //glVertex3f(0, 0, height);

			pointCount++;
			int fanPointIndex = totalPointCount + pointCount;

			for (int slice_count = dualslices; slice_count >= 0; --slice_count) {
				vec3 &p = verts[slice_count + (stacks)*(dualslices + 1)];
				points.push_back(vec3(p.x, p.y, p.z)); //glVertex3f(p.x, p.y, p.z);

				pointCount++;
				if (pointCount >= 3) {
					createTriangle(&triangles, totalPointCount + pointCount - 1, fanPointIndex, totalPointCount + pointCount);
				}
			}
		}

		return new Geometry(points, normals, uvs, triangles, true);
	}
}

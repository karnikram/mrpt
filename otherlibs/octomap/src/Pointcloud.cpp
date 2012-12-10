// $Id: Pointcloud.cpp 269 2011-08-18 16:00:50Z kai_wurm $

/**
* OctoMap:
* A probabilistic, flexible, and compact 3D mapping library for robotic systems.
* @author K. M. Wurm, A. Hornung, University of Freiburg, Copyright (C) 2009.
* @see http://octomap.sourceforge.net/
* License: New BSD License
*/

/*
 * Copyright (c) 2009-2011, K. M. Wurm, A. Hornung, University of Freiburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University of Freiburg nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef _MSC_VER
  #include <algorithm>
#else
  #include <ext/algorithm>
#endif
#include <fstream>
#include <math.h>

#include <octomap/Pointcloud.h>

namespace octomap {


  Pointcloud::Pointcloud() {

  }

  Pointcloud::~Pointcloud() {
    this->clear();
  }

  void Pointcloud::clear() {

    // delete the points
    if (points.size()) {
      points.clear();
    }
  }


  Pointcloud::Pointcloud(const Pointcloud& other) {
    for (Pointcloud::const_iterator it = other.begin(); it != other.end(); it++) {
      points.push_back(point3d(*it));
    }
  }

  Pointcloud::Pointcloud(Pointcloud* other) {
    for (Pointcloud::const_iterator it = other->begin(); it != other->end(); it++) {
      points.push_back(point3d(*it));
    }
  }


  void Pointcloud::push_back(const Pointcloud& other)   {
    for (Pointcloud::const_iterator it = other.begin(); it != other.end(); it++) {
      points.push_back(point3d(*it));
    }
  }

  point3d Pointcloud::getPoint(unsigned int i) {
    if (i<points.size()) return points[i];
    else {
      OCTOMAP_WARNING("Pointcloud::getPoint index out of range!\n");
      return points.back();
    }
  }

  void Pointcloud::transform(octomath::Pose6D transform) {

    for (unsigned int i=0; i<points.size(); i++) {
      points[i] = transform.transform(points[i]);
    }

   // FIXME: not correct for multiple transforms
    current_inv_transform = transform.inv();
  }


  void Pointcloud::transformAbsolute(pose6d transform) {

    // undo previous transform, then apply current transform
    pose6d transf = current_inv_transform * transform;

    for (unsigned int i=0; i<points.size(); i++) {
      points[i] = transf.transform(points[i]);
    }

    current_inv_transform = transform.inv();
  }


  void Pointcloud::rotate(double roll, double pitch, double yaw) {

    for (unsigned int i=0; i<points.size(); i++) {
      points[i].rotate_IP(roll, pitch, yaw);
    }
  }


  void Pointcloud::calcBBX(point3d& lowerBound, point3d& upperBound) const {
    float min_x, min_y, min_z;
    float max_x, max_y, max_z;
    min_x = min_y = min_z = 1e6;
    max_x = max_y = max_z = -1e6;

    float x,y,z;

    for (Pointcloud::const_iterator it=begin(); it!=end(); it++) {

      x = (*it)(0);
      y = (*it)(1);
      z = (*it)(2);

      if (x < min_x) min_x = x;
      if (y < min_y) min_y = y;
      if (z < min_z) min_z = z;

      if (x > max_x) max_x = x;
      if (y > max_y) max_y = y;
      if (z > max_z) max_z = z;
    }

    lowerBound(0) = min_x; lowerBound(1) = min_y; lowerBound(2) = min_z;
    upperBound(0) = max_x; upperBound(1) = max_y; upperBound(2) = max_z;
  }


  void Pointcloud::crop(point3d lowerBound, point3d upperBound) {

    Pointcloud result;

    float min_x, min_y, min_z;
    float max_x, max_y, max_z;
    float x,y,z;

    min_x = lowerBound(0); min_y = lowerBound(1); min_z = lowerBound(2);
    max_x = upperBound(0); max_y = upperBound(1); max_z = upperBound(2);

    for (Pointcloud::const_iterator it=begin(); it!=end(); it++) {
      x = (*it)(0);
      y = (*it)(1);
      z = (*it)(2);

      if ( (x >= min_x) &&
	   (y >= min_y) &&
	   (z >= min_z) &&
	   (x <= max_x) &&
	   (y <= max_y) &&
	   (z <= max_z) ) {
	result.push_back (x,y,z);
      }
    } // end for points

    this->clear();
    this->push_back(result);

  }


  void Pointcloud::minDist(double thres) {
    Pointcloud result;

    float x,y,z;
    for (Pointcloud::const_iterator it=begin(); it!=end(); it++) {
      x = (*it)(0);
      y = (*it)(1);
      z = (*it)(2);
      double dist = sqrt(x*x+y*y+z*z);
      if ( dist > thres ) result.push_back (x,y,z);
    } // end for points
    this->clear();
    this->push_back(result);
  }


  void Pointcloud::subSampleRandom(unsigned int num_samples, Pointcloud& sample_cloud) {
    point3d_collection samples;
    // visual studio does not support random_sample_n
  #ifdef _MSC_VER
    samples.reserve(this->size());
    samples.insert(samples.end(), this->begin(), this->end());
    std::random_shuffle(samples.begin(), samples.end());
    samples.resize(num_samples);
  #else
    random_sample_n(begin(), end(), std::back_insert_iterator<point3d_collection>(samples), num_samples);
    for (unsigned int i=0; i<samples.size(); i++) {
      sample_cloud.push_back(samples[i]);
    }
  #endif
  }


  void Pointcloud::writeVrml(std::string filename){

    std::ofstream outfile (filename.c_str());

    outfile << "#VRML V2.0 utf8" << std::endl;
    outfile << "Transform {" << std::endl;
    outfile << "translation 0 0 0" << std::endl;
    outfile << "rotation 0 0 0 0" << std::endl;
    outfile << "  children [" << std::endl;
    outfile << "     Shape{" << std::endl;
    outfile << "  geometry PointSet {" << std::endl;
    outfile << "      coord Coordinate {" << std::endl;
    outfile << "          point [" << std::endl;

    OCTOMAP_DEBUG_STR("PointCloud::writeVrml writing "
	      << points.size() << " points to " 
	      << filename.c_str() <<  ".");

    for (unsigned int i = 0; i < (points.size()); i++){
      outfile << "\t\t" << (points[i])(0) 
	      << " " <<    (points[i])(1) 
	      <<  " " <<   (points[i])(2) 
	      << "\n";
    }

    outfile << "                 ]" << std::endl;
    outfile << "      }" << std::endl;
    outfile << "    color Color{" << std::endl;
    outfile << "              color [" << std::endl;

    for (unsigned int i = 0; i < points.size(); i++){
      outfile << "\t\t 1.0 1.0 1.0 \n";
    }

    outfile << "                 ]" << std::endl;
    outfile << "      }" << std::endl;

    outfile << "   }" << std::endl;
    outfile << "     }" << std::endl;


    outfile << "  ]" << std::endl;
    outfile << "}" << std::endl;


  }

  std::istream& Pointcloud::read(std::istream &s){
    while (!s.eof()){
      point3d p;
      for (unsigned int i=0; i<3; i++){
        s >> p(i);
      }
      if (!s.fail()){
        this->push_back(p);
      } else {
        break;
      }
    }

    return s;
  }

  std::istream& Pointcloud::readBinary(std::istream &s) {

    unsigned int pc_size = 0;
    s.read((char*)&pc_size, sizeof(pc_size));
    OCTOMAP_DEBUG("Reading %d points from binary file...", pc_size);

    if (pc_size > 0) {
      this->points.reserve(pc_size);
      point3d p;
      for (unsigned int i=0; i<pc_size; i++) {
        p.readBinary(s);
        if (!s.fail()) {
          this->push_back(p);
        }
        else {
          OCTOMAP_ERROR("Pointcloud::readBinary: ERROR.\n" );
          break;
        }
      }
    }
    OCTOMAP_DEBUG("done.\n");

    return s;
  }


  std::ostream& Pointcloud::writeBinary(std::ostream &s) const {

    size_t pc_size = this->size();
    OCTOMAP_DEBUG("Writing %d points to binary file...", pc_size);
    s.write((char*)&pc_size, sizeof(pc_size));

    for (Pointcloud::const_iterator it = this->begin(); it != this->end(); it++) {
      it->writeBinary(s);
    }
    OCTOMAP_DEBUG("done.\n");

    return s;
  }

} // end namespace

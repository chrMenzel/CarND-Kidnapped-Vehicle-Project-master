/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;

// declaring a random engine
std::default_random_engine gen;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  num_particles = 200;  // TODO: Set the number of particles
  weights.resize(num_particles);

  // These line creates a normal (Gaussian) distribution for x, y and theta
  std::normal_distribution<double> distX(x, std[0]);
  std::normal_distribution<double> distY(y, std[1]);
  std::normal_distribution<double> distTheta(theta, std[2]);

  // init particles
  for (int i = 0; i < num_particles; i++) {
    Particle p;

    p.id = i;
    p.x = distX(gen);
    p.y = distY(gen);
    p.theta = distTheta(gen);
    p.weight = 1.0;
    
    particles.push_back(p);
    weights.push_back(p.weight);
  }

  is_initialized = true;

}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */

  for (int i = 0; i < num_particles; i++) {
    double particleX = particles[i].x;
    double particleY = particles[i].y;
    double particleTheta = particles[i].theta;
      
    double predX, predY, predTheta;

    // When yaw rate is not equal to zero, we have to use another calculation
    if (fabs(yaw_rate) > 0.0001) {
      predX = particleX + (velocity/yaw_rate) * (sin(particleTheta + (yaw_rate * delta_t)) - sin(particleTheta));
      predY = particleY + (velocity/yaw_rate) * (cos(particleTheta) - cos(particleTheta + (yaw_rate * delta_t)));
      predTheta = particleTheta + (yaw_rate * delta_t);
    } else {
      predX = particleX + velocity * cos(particleTheta) * delta_t;
      predY = particleY + velocity * sin(particleTheta) * delta_t;
      predTheta = particleTheta;
    }
      
    std::normal_distribution<double> dist_x(predX, std_pos[0]);
    std::normal_distribution<double> dist_y(predY, std_pos[1]);
    std::normal_distribution<double> dist_theta(predTheta, std_pos[2]);
      
    particles[i].x = dist_x(gen);
    particles[i].y = dist_y(gen);
    particles[i].theta = dist_theta(gen);
  }

}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  for (unsigned int i = 0; i < observations.size(); i++) {
    
    // grab current observation
    LandmarkObs o = observations[i];

    // initialize minimum distance to maximum possible
    double minDistance = std::numeric_limits<double>::max();
    // initialize id of landmark
    int closestLandmarkID = -1;
    
    for (unsigned int j = 0; j < predicted.size(); j++) {
      
      // get distance between current/predicted landmarks (dist is a function in helper_functions)
      double currentDistance = dist(o.x, o.y, predicted[j].x, predicted[j].y);

      // find the predicted landmark nearest the current observed landmark
      if (currentDistance < minDistance) {
        minDistance = currentDistance;
        closestLandmarkID = predicted[j].id;
      }
    }

    observations[i].id = closestLandmarkID;
  }
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a multi-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */

  weights.clear();

  for (int i = 0; i < num_particles; i++) {

    /* Step 1: Transform observations from vehicle coordinates to map coordinates.*/
    // create a vector to hold the predicted map landmark locations
    vector<LandmarkObs> transformedObservations;

    double particleX = particles[i].x;
    double particleY = particles[i].y;
    double particleTheta = particles[i].theta;

    for (unsigned int j = 0; j < observations.size(); j++) {
      LandmarkObs transformedObs;
      transformedObs.id = j;
      transformedObs.x = particleX + (cos(particleTheta) * observations[j].x) - (sin(particleTheta) * observations[j].y);
      transformedObs.y = particleY + (sin(particleTheta) * observations[j].x) + (cos(particleTheta) * observations[j].y);
      transformedObservations.push_back(transformedObs);
    }

    /* Step 2: Keep the landmarks which are in the sensor range of the current particle. */
    vector<LandmarkObs> predictedLandmarks;

    for (unsigned int j = 0; j < map_landmarks.landmark_list.size(); j++) {
			double landmarkDistance = dist(particles[i].x, particles[i].y, map_landmarks.landmark_list[j].x_f, map_landmarks.landmark_list[j].y_f);

			if (landmarkDistance<sensor_range) {
				LandmarkObs predictedLandmark;
				predictedLandmark.id = map_landmarks.landmark_list[j].id_i;
				predictedLandmark.x = map_landmarks.landmark_list[j].x_f;
				predictedLandmark.y = map_landmarks.landmark_list[j].y_f;
        predictedLandmarks.push_back(predictedLandmark);
      }
    }

    /* Step 3: Associate observations to predicted landmarks using nearest neighbor algorithm and calculate 
               the weight of each particle using Multivariate Gaussian distribution.
    */
    dataAssociation(predictedLandmarks, transformedObservations);

    /* Step 4: Calculate and normalize the weights of all particles */
		double probability = 1;

		for (unsigned int j = 0; j < predictedLandmarks.size(); j++) {
			int minimumID = -1;
      double minimumDistance = std::numeric_limits<double>::max();

			for (unsigned int k = 0; k < transformedObservations.size(); k++) {
				double distance = dist(predictedLandmarks[j].x, predictedLandmarks[j].y, transformedObservations[k].x, transformedObservations[k].y);

				if (distance < minimumDistance){
					minimumDistance = distance;
					minimumID = k;
				}
			}

			if (minimumID != -1){
				probability *= exp(-((predictedLandmarks[j].x - transformedObservations[minimumID].x) * 
                       (predictedLandmarks[j].x - transformedObservations[minimumID].x) / 
                       (2 * std_landmark[0] * std_landmark[0]) +
                       (predictedLandmarks[j].y - transformedObservations[minimumID].y) *
                       (predictedLandmarks[j].y - transformedObservations[minimumID].y) /
                       (2 * std_landmark[1] * std_landmark[1]))) /
                       (2.0 * M_PI * std_landmark[0] * std_landmark[1]);

			}
		}

		weights.push_back(probability);
		particles[i].weight = probability;

  }

}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
	vector<Particle> resampled_particles;
	
	// Generate random particle index for resampling wheel
	std::uniform_int_distribution<int> particle_index(0, num_particles - 1);
	int index = particle_index(gen);
	double beta = 0.0;
	double maximumWeight = *max_element(weights.begin(), weights.end()) * 2.0;  // To save calculation time
  // uniform random distribution [0.0, maximumWeight)
	std::uniform_real_distribution<double> uniformRndDistribution(0.0, maximumWeight);

  // Spin the reasampling wheel
	for (int i = 0; i < num_particles; i++) {
    // uniform random distribution [0.0, maximumWeight)
	  std::uniform_real_distribution<double> uniformRndDistribution(0.0, maximumWeight);
		beta += uniformRndDistribution(gen);

	  while (beta > weights[index]) {
	    beta -= weights[index];
	    index = (index + 1) % num_particles;
	  }
	  resampled_particles.push_back(particles[index]);
	}
	particles = resampled_particles;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}
#include <uWS/uWS.h>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "spline.h"

// for convenience
using nlohmann::json;
using std::string;
using std::vector;

bool first_flag = true;
int my_lane = 1;
double new_velocity = 0.0;

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  std::ifstream in_map_(map_file_.c_str(), std::ifstream::in);

  string line;
  while (getline(in_map_, line)) {
    std::istringstream iss(line);
    double x;
    double y;
    float s;
    float d_x;
    float d_y;
    iss >> x;
    iss >> y;
    iss >> s;
    iss >> d_x;
    iss >> d_y;
    map_waypoints_x.push_back(x);
    map_waypoints_y.push_back(y);
    map_waypoints_s.push_back(s);
    map_waypoints_dx.push_back(d_x);
    map_waypoints_dy.push_back(d_y);
  }
  
  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,
               &map_waypoints_dx,&map_waypoints_dy]
              (uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
               uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
          // Main car's localization Data
          double car_x = j[1]["x"];
          double car_y = j[1]["y"];
          double car_s = j[1]["s"];
          double car_d = j[1]["d"];
          double car_yaw = j[1]["yaw"];
          double car_speed = j[1]["speed"];

          // Previous path data given to the Planner
          auto previous_path_x = j[1]["previous_path_x"];
          auto previous_path_y = j[1]["previous_path_y"];
          // Previous path's end s and d values 
          double end_path_s = j[1]["end_path_s"];
          double end_path_d = j[1]["end_path_d"];

          // Sensor Fusion Data, a list of all other cars on the same side 
          //   of the road.
          auto sensor_fusion = j[1]["sensor_fusion"];

          json msgJson;

          vector<double> next_x_vals;
          vector<double> next_y_vals;
		  
          tk::spline spl;
          
          /**
           * TODO: define a path made up of (x,y) points that the car will visit
           *   sequentially every .02 seconds
           */
          
          for(int i = 0; i < previous_path_x.size(); i++)
          {
            next_x_vals.push_back(previous_path_x[i]);
            next_y_vals.push_back(previous_path_y[i]);
          }
          
          if(!first_flag){
            car_s = end_path_s;
          }
		  
          bool other_vehicle_on_left = false;
          bool other_vehicle_on_right = false;
		  bool other_vehicle_on_center = false;
          
          for(int i = 0; i < sensor_fusion.size(); i++)
          {
            double other_vehicle_x = sensor_fusion[i][3];
            double other_vehicle_y = sensor_fusion[i][4];
            double other_vehicle_s = sensor_fusion[i][5];
            int other_vehicle_d = sensor_fusion[i][6];
            int other_vehicle_lane;
            
            if(other_vehicle_d > 0 && other_vehicle_d < 4){
              other_vehicle_lane = 0;
            }
            else if(other_vehicle_d > 4 && other_vehicle_d < 8){
              other_vehicle_lane = 1;
            }
            else if(other_vehicle_d > 8 && other_vehicle_d < 12){
              other_vehicle_lane = 2;
            }
            else{
              other_vehicle_lane = -1;
            }
             
            double other_vehicle_velo = sqrt(pow(other_vehicle_x, 2) + pow(other_vehicle_y, 2));     
            other_vehicle_s = other_vehicle_s + other_vehicle_velo * 0.02 * previous_path_x.size();
              
            if (other_vehicle_lane == my_lane && other_vehicle_s - car_s < 35 && other_vehicle_s - car_s > 0) {
              other_vehicle_on_center = true;
            }
            else if (my_lane - other_vehicle_lane == 1 && other_vehicle_s - car_s < 40 && car_s - other_vehicle_s < 15){
              other_vehicle_on_left = true;
            }
            else if (other_vehicle_lane - my_lane == 1 && other_vehicle_s - car_s < 40 && car_s - other_vehicle_s < 15) {
              other_vehicle_on_right = true;
            }
          }
           
          if(other_vehicle_on_center && my_lane != 0 && !other_vehicle_on_left){
            my_lane = my_lane - 1;
          }
          else if(other_vehicle_on_center && my_lane != 2 && !other_vehicle_on_right){
            my_lane = my_lane + 1;
          }
          else if(other_vehicle_on_center){
            new_velocity = new_velocity - 1;  
          }  
          else if(new_velocity < 49.6){
            new_velocity = new_velocity + 0.3;
          }
            
          vector<double> spline_x;
          vector<double> spline_y;

          double previous_x;
          double previous_y;
          double pre_previous_x;
          double pre_previous_y;
          double previous_raw;
          
          if(first_flag){
            previous_x = car_x;
            previous_y = car_y;
            pre_previous_x = car_x - cos(car_yaw);
            pre_previous_y = car_y - sin(car_yaw);
            previous_raw = deg2rad(car_yaw);
          }
        
          else{
            previous_x = previous_path_x[previous_path_x.size() - 1];
            previous_y = previous_path_y[previous_path_x.size() - 1];
            pre_previous_x =  previous_path_x[previous_path_x.size() - 2];
            pre_previous_y =  previous_path_y[previous_path_x.size() - 2];
            previous_raw = atan2(previous_y - pre_previous_y, previous_x - pre_previous_x);
          }
          
          spline_x.push_back(pre_previous_x);
          spline_x.push_back(previous_x);
          spline_y.push_back(pre_previous_y);
          spline_y.push_back(previous_y);
          
          spline_x.push_back(getXY(car_s + 25, (2+4*my_lane), map_waypoints_s, map_waypoints_x, map_waypoints_y)[0]);
          spline_y.push_back(getXY(car_s + 25, (2+4*my_lane), map_waypoints_s, map_waypoints_x, map_waypoints_y)[1]);
		  
          double x_temp;
          double y_temp;
          
          for(int i = 0; i < spline_x.size(); ++i){
            x_temp = spline_x[i] - previous_x;
            y_temp = spline_y[i] - previous_y;
            spline_x[i] = x_temp * cos(-previous_raw) - y_temp * sin(-previous_raw);
            spline_y[i] = x_temp * sin(-previous_raw) + y_temp * cos(-previous_raw);
          }
          
          spl.set_points(spline_x, spline_y);

          x_temp = 0.0;
          y_temp = 0.0;
          
          for(int i = 0; i <= 50 - previous_path_x.size(); ++i)
          {
            x_temp = x_temp + (new_velocity * 0.02)/2.24;
            y_temp = spl(x_temp);
            double final_x = x_temp * cos(previous_raw) - y_temp * sin(previous_raw);
            double final_y = x_temp * sin(previous_raw) + y_temp * cos(previous_raw);

            final_x += previous_x;
            final_y += previous_y;

            next_x_vals.push_back(final_x);
            next_y_vals.push_back(final_y);
          }
          
          first_flag = false;
          
          //TODO end
          
          msgJson["next_x"] = next_x_vals;
          msgJson["next_y"] = next_y_vals;

          auto msg = "42[\"control\","+ msgJson.dump()+"]";

          ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
        }  // end "telemetry" if
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }  // end websocket if
  }); // end h.onMessage

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  
  h.run();
}
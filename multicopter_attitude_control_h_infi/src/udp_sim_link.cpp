#include <chrono>
#include <thread>
#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include "multirotor_attitude_control_h_infi.hpp"

using boost::asio::ip::udp;

int main(int argc, char* argv[])
{
	//TODO: Parse inputs
	std::string str_internal_ip = "127.0.0.1";
	try
	{
		// if (argc != 2)
		// {
		// 	std::cerr << "Usage: client <host>" << std::endl;
		// 	return 1;
		// }
		std::cout<< "Started" << std::endl;
		// Set up UDP communication
		boost::asio::ip::address internal_ip = boost::asio::ip::address::from_string(str_internal_ip);
		boost::asio::io_service io_service;
		
		udp::endpoint endpoint_recv_from(internal_ip,2040);
		udp::endpoint endpoint_send_to(internal_ip,2500);
	       
		udp::socket socket_recv(io_service);
		udp::socket socket_send(io_service);
		socket_recv.open(udp::v4());
		socket_send.open(udp::v4());
		socket_recv.bind(endpoint_recv_from);
		//socket_send.bind(endpoint_send_to);
		// Initialize our controller
		Multirotor_Attitude_Control_H_Infi quad_control;
		quad_control.set_phys_params(8.0f*6.228e-3f, 8.0f*6.228e-3, 8.0f*1.121e-2f);
		quad_control.set_weights(0.0001f,0.0001f,0.0001f,10000.0f);
		quad_control.set_mode(true, true, true);
		// Set up a bunch of parameters for measurement reading and control sending
		Multirotor_Attitude_Control_H_Infi::State meas_state, meas_rate, torque_out;
		boost::array<double, 40> recv_buf;
		boost::array<double, 3> send_buf;
		bool sim_running = true;
		std::cout<<"Control ready for input"<<std::endl;
		while (sim_running) {
			// Get state information for roll, pitch and yaw and their derivs
			size_t len =socket_recv.receive_from(
				boost::asio::buffer(recv_buf), endpoint_recv_from);
			printf( "RECV: '%lu' %e %e %e %e %e %e\n", len, recv_buf[0],recv_buf[1],recv_buf[2],recv_buf[3],recv_buf[4],recv_buf[5] );
			meas_state.r = recv_buf[0];
			meas_state.p = recv_buf[1];
			meas_state.y = recv_buf[2];
			meas_rate.r = recv_buf[3];
			meas_rate.p = recv_buf[4];
			meas_rate.y = recv_buf[5];
			// Update the control command
			quad_control.control(meas_state, meas_rate, torque_out);
			// Send control to the simulation
			send_buf[0]=torque_out.r;
			send_buf[1]=torque_out.p;
			send_buf[2]=torque_out.y;
			printf( "SENT: %e %e %e\n", send_buf[0], send_buf[1], send_buf[2] );
			socket_send.send_to(boost::asio::buffer(send_buf), endpoint_send_to);
			}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}

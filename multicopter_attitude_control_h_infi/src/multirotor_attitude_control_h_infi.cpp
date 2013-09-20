#include <stdint.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#include <vmmlib/matrix.hpp>
#include <vmmlib/vector.hpp>

#include "multirotor_attitude_control_h_infi.hpp"

//DEBUG
#include <iostream>
typedef vmml::matrix< 3, 3, float> Matrix;
typedef vmml::vector<3,float> Vector;

Multirotor_Attitude_Control_H_Infi::Multirotor_Attitude_Control_H_Infi() {
	_last_run = 0;
	_tc = 0.1f;
	_weight_error_deriv = 1;
	_weight_error_state = 1;
	_weight_error_integral = 1;
	_weight_torque = 1;
	_Ixx =1;
	_Iyy =1;
	_Izz =1;
	_setpoint_state = State();
	_setpoint_rate = State();
	_setpoint_accel = State();
	//_command_torque [3];
	_modes_set = false;
	_state_track = false;
	_rate_track = false;
	_accel_track= false;
}

void Multirotor_Attitude_Control_H_Infi::set_mode(bool state_track, bool rate_track, bool accel_track){
	_state_track = state_track;
	_rate_track = rate_track;
	_accel_track= accel_track;
	_modes_set = true;
}
void Multirotor_Attitude_Control_H_Infi::set_setpoints(const State& state,const State& rate,const State& accel) {
	_setpoint_state = state;
	_setpoint_rate = rate;
	_setpoint_accel = accel;
}
void Multirotor_Attitude_Control_H_Infi::control(const State& meas_state, const State& meas_rate, State& torque_out) {
	if(!_modes_set) {
		std::cout << "Error, modes have not been set yet" << std::endl;
		return;
	}
	// TODO: check inputs here
	Vector k_p;
	Vector k_i;
	Vector k_d;
	make_M(meas_state,_M);
	make_C(meas_state, meas_rate, _Cor);
	calc_gains(_M,_Cor, k_p, k_i, k_d);
	printf("M(1,1) %e, Cor(1,1) %e, k_p(1) %e, k_i(1) %e, k_d(1) %e\n",_M(1,1),_Cor(1,1),k_p(1),k_i(1),k_d(1));
	Vector error_state;
	if( _state_track ){
		error_state(0) = meas_state.r-_setpoint_state.r;
		error_state(1) = meas_state.p-_setpoint_state.p;
		error_state(2) = meas_state.y-_setpoint_state.y;
	}else{
		error_state = Vector::ZERO;
	}
	Vector error_rate;
	if( _rate_track ) {
		error_rate(0) = meas_rate.r-_setpoint_rate.r;
		error_rate(1) = meas_rate.p-_setpoint_rate.p;
		error_rate(2) = meas_rate.y-_setpoint_rate.y;
	} else {
		error_rate = Vector::ZERO;
	}
	Vector setpoint_accel;
	if( _accel_track ) {
		setpoint_accel(0) = _setpoint_accel.r;
		setpoint_accel(1) = _setpoint_accel.p;
		setpoint_accel(2) = _setpoint_accel.y;
	} else {
		setpoint_accel = Vector::ZERO;
	}
	Vector meas_rate_vect;
	meas_rate_vect(0)=meas_state.r;
	meas_rate_vect(1)=meas_state.p;
	meas_rate_vect(2)=meas_state.y;
	printf("ERate(1)  %e, EState(1) %e, int(1) %e\n",error_rate(1),error_state(1),_integral(1));
	_integral = _integral + error_state;
	// TODO: check integral limits and saturation
	Vector control_accel = k_d*error_rate + k_p*error_state + k_i*_integral;
	Vector control_torque = _M*setpoint_accel + _Cor*meas_rate_vect - _M*control_accel;
	torque_out.r = control_torque(0);
	printf("torque(0) %e\n",control_torque(0));
	torque_out.p = control_torque(1);
	torque_out.y = control_torque(2);
}

void Multirotor_Attitude_Control_H_Infi::reset_integrator()
{
	_integral(0)=0.0f;
	_integral(1)=0.0f;
	_integral(2)=0.0f;
}

void Multirotor_Attitude_Control_H_Infi::calc_gains(const Matrix& M,const Matrix& C, Vector& k_p, Vector& k_i, Vector& k_d) {
	Matrix I;
	I(0,0)=1.0f;I(1,1)=1.0f;I(2,2)=1.0f;
	Matrix M_inv;
	M.inverse(M_inv);
	Matrix Dynamics_weights = M_inv*(C+I*(1.0f/_weight_torque) );
	float long_expr = std::sqrt(_weight_error_state*_weight_error_state + 2.0f*_weight_error_deriv*_weight_error_integral)/_weight_error_state;

	k_d=(I*long_expr)+Dynamics_weights;
	k_p=I*(_weight_error_integral/_weight_error_deriv)+Dynamics_weights*long_expr;
	k_i=Dynamics_weights*(_weight_error_integral/_weight_error_deriv);
}

void Multirotor_Attitude_Control_H_Infi::make_M(const State& St, Matrix& M) {
	float M_vals [9] = {0};
	float sin_R=std::sin(St.r);
	float cos_R=std::cos(St.r);
	float sin_P=std::sin(St.p);
	float cos_P=std::cos(St.p);
	//First Row
	M_vals[0]=_Ixx;
	M_vals[1]=0;
	M_vals[2]=-_Ixx*sin_P;
	// Second Row
	M_vals[3]=_Iyy*cos_R*cos_R+_Izz*sin_R*sin_R;
	M_vals[4]=(_Iyy-_Izz)*cos_R*sin_R*sin_P;
	M_vals[5]=0;
	// Third row
	M_vals[6]=-_Ixx*sin_P;
	M_vals[7]=(_Iyy-_Izz)*cos_R*sin_R*cos_P;
	M_vals[8]=_Ixx*sin_P*sin_P + _Iyy*sin_R*sin_R*cos_P*cos_P +
		     _Izz*cos_R*cos_R*cos_P*cos_P;

	M.set(M_vals,M_vals+9);
}

void Multirotor_Attitude_Control_H_Infi::make_C(const State& St, const State& Rate, Matrix& C) {
	float C_vals [9] = {0};
	float sin_R=std::sin(St.r);
	float cos_R=std::cos(St.r);
	float sin_P=std::sin(St.p);
	float cos_P=std::cos(St.p);
	float long_factor = Rate.p*cos_R*sin_R + Rate.y*sin_R*sin_R*cos_P;
	//First Row
	C_vals[0]=0;
	C_vals[1]=(_Iyy-_Izz)*(long_factor) + 
		     (_Izz-_Iyy)*Rate.y*cos_R*cos_R*cos_P -
		     _Ixx*Rate.y*cos_P;
	C_vals[2]=(_Izz-_Iyy)*Rate.p*cos_R*sin_R*cos_P*cos_P;
	//Second Row
	C_vals[3]=(_Izz-_Iyy)*(long_factor) + 
	             (_Iyy-_Izz)*Rate.y*cos_R*cos_R*cos_P +
		     _Ixx*Rate.y*cos_P;
	C_vals[4]=(_Izz-_Iyy)*Rate.r*cos_R*cos_R;
	C_vals[5]=-_Ixx*Rate.y*sin_P*cos_P +
		      _Iyy*Rate.y*sin_R*sin_R*cos_P*sin_P +
		      _Izz*Rate.y*cos_R*cos_R*sin_P*cos_P;
	//Third Row
	C_vals[6]=(_Iyy-_Izz)*Rate.y*cos_P*cos_P*sin_R*cos_R - 
		     _Ixx*Rate.p*cos_P;
	C_vals[7]=(_Izz-_Iyy)*(Rate.p*cos_R*sin_R*sin_P+Rate.r*sin_R*sin_R*cos_P) +
		     (_Iyy-_Izz)*Rate.r*cos_R*cos_R*cos_P + 
		     _Ixx*Rate.y*sin_P*cos_P - 
		     _Iyy*Rate.y*sin_R*sin_R*sin_P*cos_P - 
		     _Izz*Rate.y*cos_R*cos_R*sin_P*cos_P;
	C_vals[8]=(_Iyy-_Izz)*Rate.r*cos_R*sin_R*cos_P*cos_P - 
		     _Iyy*Rate.p*sin_R*sin_R*cos_P*sin_P - 
		     _Izz*Rate.p*cos_R*cos_R*cos_P*sin_P + 
		     _Ixx*Rate.p*cos_P*sin_P;
	C.set( C_vals, C_vals + 9 );
}

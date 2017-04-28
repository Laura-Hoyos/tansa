#include <tansa/simulate.h>

#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include <iostream>
using namespace std;

namespace tansa {


Firmware::Firmware(const DataObject &desc, MultirotorModel::Ptr model) {
	currentActuatorOutputs.resize(4, 0);

	this->id = desc["id"];
	json d = desc;
	this->rcScript = d["rc"];

	process = 0;

	this->model = model;
	model->imu->subscribe(&Firmware::onImuData, this);
	model->gps->subscribe(&Firmware::onGpsData, this);
}

Firmware::~Firmware() {
	this->stop();
}

void Firmware::start() {
	int p = fork();
	if(p == 0) { // Child
		char *const bash = (char *const) "/bin/bash";
		char *const script = (char *const) "scripts/start_instance.sh";
		char *const rc_script = (char *const) this->rcScript.c_str();
		char num[16];
		strcpy(num, std::to_string(this->id).c_str());

		char *const argv[] = { bash, script, num, rc_script, NULL};

		execv(bash, argv);

		exit(0);
		return;
	}

	this->process = p;

	this->sim_vehicle = new Vehicle();
	this->sim_vehicle->connect(0, 14561 + 10*id);
	this->sim_vehicle->subscribe(&Firmware::onActuatorOutputs, this);
}

void Firmware::stop() {
	if(process == 0)
		return;

	kill(process, SIGKILL);

	waitpid(process, NULL, 0);

	//this->sim_vehicle->disconnect();
}

void Firmware::connectClient(Vehicle *v) {
	// These should match 'scripts/start_instance.sh'
	int lport = 14550 + 10*id,
		rport = 14555 + 10*id;

	v->connect(lport, rport);
}

void Firmware::onImuData(const IMUSensorData *data) {
	sim_vehicle->hil_sensor(&data->accel, &data->gyro, &data->mag, Time::now()); // data->time);
}

void Firmware::onGpsData(const GPSData *data) {
	sim_vehicle->hil_gps(data->latLongAlt, data->vel, data->time);
}

void Firmware::onMocapData(const MocapSensorData *data) {
	// TODO: The main issue with this is that we'd like to be able to direct the mocap data to the vehicle used by the controller as that one will be used for
}

void Firmware::onActuatorOutputs(const ActuatorOutputs *actuators) {
	currentActuatorOutputs.resize(4);
	for(int i = 0; i < 4; i++) {
	 	currentActuatorOutputs[i] = actuators->outputs[i];
	}
}

void Firmware::update(State::Ptr _s) {
	std::shared_ptr<MultirotorModelState> s = std::static_pointer_cast<MultirotorModelState>(_s);

	for(int i = 0; i < 4; i++) {
		model->motors[i]->control(s->motors[i], currentActuatorOutputs[i]);
	}
}


}

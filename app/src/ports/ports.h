#pragma once
#include "../app_types.h"

struct InputPorts {
  struct DriveVelocityCommand drive_vel_cmd;
  struct DrivePositionCommand drive_pos_cmd;
  struct SWSafetyCommands safety_cmd;
};

struct OutputPorts {
  struct MotorStates motor_states;
  struct ArmMotorStates arm_states;
  struct PeripheralTelemetry sensor_states;
  struct SafetyState safety_states;
};

struct VCUPorts {
  struct InputPorts in;
  struct OutputPorts out;
};

int ports_init(struct VCUPorts *ports);
int ports_read_inputs(struct VCUPorts *ports);
int ports_write_outputs(struct VCUPorts *ports);

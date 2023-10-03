#include <Arduino.h>
#include <lx16a-servo.h>
#include <WiFi.h>
#include <iostream>
#include <string>

// ROS Includes
#include <micro_ros_platformio.h>
#include <rclc/rclc.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

// ROS msg includes
#include <std_msgs/msg/int64_multi_array.h>
#include <std_msgs/msg/multi_array_dimension.h>
#include <std_msgs/msg/string.h>
#include <sensor_msgs/msg/joint_state.h>
#include <std_msgs/msg/int32.h>
#include <geometry_msgs/msg/pose_array.h>

#include <rosidl_runtime_c/string.h>
#include <rosidl_runtime_c/string_functions.h>
#include <rosidl_runtime_c/primitives_sequence.h>


#if !defined(MICRO_ROS_TRANSPORT_ARDUINO_SERIAL)
#error This example is only avaliable for Arduino framework with serial transport.
#endif

// =====================================================================================================
// GLOBALS
// =====================================================================================================

// Servos and Servo Bus Instantiation
LX16ABus servoBus;
LX16AServo servo1(&servoBus, 1); // 0-16800 (0-168 degrees)
LX16AServo servo2(&servoBus, 2); // 0-24000 (0-240 degrees)
LX16AServo servo3(&servoBus, 3); // 0-24000 (0-240 degrees)
LX16AServo servo4(&servoBus, 4); // 0-24000 (0-240 degrees)
LX16AServo servo5(&servoBus, 5); // 0-24000 (0-240 degrees)
LX16AServo servo6(&servoBus, 6); // 0-24000 (0-240 degrees)

// Create string sequence
rosidl_runtime_c__String__Sequence name__string_sequence;

// // Array to hold latest read servo parameters
// double vin[6] = {0, 0, 0, 0, 0, 0};				// servo voltages
// double temp[6] = {0, 0 ,0 ,0 ,0 ,0}; 			// servo temperatures

int8_t v_in_cached[6] = {0,0,0,0,0,0};
int8_t temp_cached[6] = {0,0,0,0,0,0};

// Array to populate JointState message for Servo positions
double pos[6] = {0, 0, 0, 0, 0, 0};				// servo positions
double effort_array[] = {0, 0, 0, 0, 0, 0};		// effort array (just needed to populate message component)
double vel[] = {0, 0, 0, 0, 0, 0};				// velocity array "  "

// Timer settings
static const uint16_t timer_divider = 80;
static const uint64_t timer_max_count = 500000;

static hw_timer_t *timer = NULL;

bool publish_servo_pos_flag = false;


rcl_node_t node_hiwonder;	// Node object

// Publisher and Subscriber objects
rcl_publisher_t 	publisher;
rcl_publisher_t 	publisher_servo_pos;

rcl_subscription_t 	subscriber;
rcl_subscription_t 	subscriber_one_servo_cmd;
rcl_subscription_t 	subscriber_multi_servo_cmd;

// ROS check definitions
rclc_support_t 	support;
rcl_allocator_t allocator;

rcl_service_t 	service;
rcl_wait_set_t 	wait_set;


// Executors
rclc_executor_t executor;
rclc_executor_t executor_subscriber;
rclc_executor_t executor_servo_pos_publish;
rclc_executor_t executor_single_servo_cmd_sub;
rclc_executor_t executor_multi_servo_move_cmd_sub;

// ROS Error Handling
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}


// Error loop (will flash LED if error)
void error_loop(){
  while(1){
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(50);
  }
}


// ROS messages
sensor_msgs__msg__JointState servo_position_array_msg;
std_msgs__msg__Int32 msg;

std_msgs__msg__Int64MultiArray single_servo_cmd_msg_in;
std_msgs__msg__Int64MultiArray multi_servo_cmd_msg_in;


// example_interfaces__srv__AddTwoInts_Response res;
// example_interfaces__srv__AddTwoInts_Request req;


// =====================================================================================================
// INTERUPT SERVICE ROUTINES
// =====================================================================================================

void IRAM_ATTR onTimer() {
	// Toggle LED
	int pin_state = digitalRead(LED_BUILTIN);
	digitalWrite(LED_BUILTIN, !pin_state);

	publish_servo_pos_flag = true;

	
}


// =====================================================================================================
// CALLBACKS
// =====================================================================================================

// // Subscriber callback for commanding single servo
// void subscription_callback_single_servo(const void * msgin) {  
// 	// digitalWrite(LED_BUILTIN, LOW);
// 	// delay(200);
// 	// digitalWrite(LED_BUILTIN,HIGH);
// 	// delay(200);
// 	// digitalWrite(LED_BUILTIN, LOW);
// 	// delay(200);
// 	// digitalWrite(LED_BUILTIN,HIGH);

// 	const std_msgs__msg__Int64MultiArray * one_servo = (const std_msgs__msg__Int64MultiArray *)msgin;

// 	// TODO: Add condition that servo pos is positive (i.e. dont do anything if it negative)
// 	if (one_servo->data.data[0] == 1) {
// 		servo1.move_time(one_servo->data.data[1], one_servo->data.data[2]);
// 	}
// 	else if (one_servo->data.data[0] == 2) {
// 		servo2.move_time(one_servo->data.data[1], one_servo->data.data[2]);
// 	}
// 	else if (one_servo->data.data[0] == 3) {
// 		servo3.move_time(one_servo->data.data[1], one_servo->data.data[2]);
// 	}
// 	else if (one_servo->data.data[0] == 4) {
// 		servo4.move_time(one_servo->data.data[1], one_servo->data.data[2]);
// 	}
// 	else if (one_servo->data.data[0] == 5) {
// 		servo5.move_time(one_servo->data.data[1], one_servo->data.data[2]);
// 	}
// 	else if(one_servo->data.data[0] == 6) {
// 		servo6.move_time(one_servo->data.data[1], one_servo->data.data[2]);
// 	}
// 	else {
// 		digitalWrite(LED_BUILTIN, LOW);
// 		delay(200);
// 		digitalWrite(LED_BUILTIN,HIGH);
// 		delay(200);
// 		digitalWrite(LED_BUILTIN, LOW);
// 		delay(200);
// 		digitalWrite(LED_BUILTIN,HIGH);
// 	}
// }

// Subscriber callback for commanding multi servo
void subscription_callback_multi_servo(const void * msgin) {
	// Move all the servos (dont move if argument is negative)
	const std_msgs__msg__Int64MultiArray * multi_servo = (const std_msgs__msg__Int64MultiArray *)msgin;
	digitalWrite(LED_BUILTIN, LOW);
	delay(200);
	digitalWrite(LED_BUILTIN,HIGH);
	delay(200);
	digitalWrite(LED_BUILTIN, LOW);
	delay(200);
	digitalWrite(LED_BUILTIN,HIGH);


	if (multi_servo->data.data[0] >= 0) {
		servo1.move_time(multi_servo->data.data[0], multi_servo->data.data[6]);
	}

	if (multi_servo->data.data[1] >= 0) {
		servo2.move_time(multi_servo->data.data[1], multi_servo->data.data[7]);
	}

	if (multi_servo->data.data[2] >= 0) {
		servo3.move_time(multi_servo->data.data[2], multi_servo->data.data[8]);
	}
	
	if (multi_servo->data.data[3] >= 0) {
		servo4.move_time(multi_servo->data.data[3], multi_servo->data.data[9]);
	}

	if (multi_servo->data.data[4] >= 0) {
		servo5.move_time(multi_servo->data.data[4], multi_servo->data.data[10]);
	}

	if (multi_servo->data.data[5] >= 0) {
		servo6.move_time(multi_servo->data.data[5], multi_servo->data.data[11]);
	}

	digitalWrite(LED_BUILTIN, LOW);
	delay(200);
	digitalWrite(LED_BUILTIN,HIGH);
	delay(200);
	digitalWrite(LED_BUILTIN, LOW);
	delay(200);
	digitalWrite(LED_BUILTIN,HIGH);
}


// =====================================================================================================
// SETUP FUNCTION
// =====================================================================================================

void setup() {
	Serial.begin(115200);
	set_microros_serial_transports(Serial);
	delay(2000);

	// LED on Init
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);
	delay(250);
	digitalWrite(LED_BUILTIN, LOW);
	delay(250);
	digitalWrite(LED_BUILTIN, HIGH);


	// =====================================================================================================
    // TIMER SETUP
    // =====================================================================================================

	// Create and start timer (num, divider, countUp)
	timer = timerBegin(0, timer_divider, true);

	// Provide ISR to timer (timer, function, edge)
	timerAttachInterrupt(timer, &onTimer, true);

	// At what count should ISR trigger (timer, count, autoreload)
	timerAlarmWrite(timer, timer_max_count, true);

	// Allow ISR to trigger
	timerAlarmEnable(timer);


	// =====================================================================================================
    // SERVO SETUP
    // =====================================================================================================

	Serial.println("Beginning Servo Example");
	servoBus.beginOnePinMode(&Serial2,33);
	servoBus.debug(true);
	servoBus.retry=3;

	// Reset the servo positions
	servo1.move_time(16000,1000);
	servo2.move_time(12000,1250);
	servo3.move_time(12000,1500);
	servo4.move_time(12000,2000);
	servo5.move_time(12000,2500);
	servo6.move_time(12000,3000);
	delay(3000);

	// Read in servo positions
	pos[0] = servo1.pos_read();
	pos[1] = servo2.pos_read();
	pos[2] = servo3.pos_read();
	pos[3] = servo4.pos_read();
	pos[4] = servo5.pos_read();
	pos[5] = servo6.pos_read();
	delay(1000);

	// // Read in servo voltages
	// vin[0] = servo1.vin();
	// vin[1] = servo2.vin();
	// vin[2] = servo3.vin();
	// vin[3] = servo4.vin();
	// vin[4] = servo5.vin();
	// vin[5] = servo6.vin();
	// delay(1000);

	// // Read in servo temperatures
	// temp[0] = servo1.temp();
	// temp[1] = servo2.temp();
	// temp[2] = servo3.temp();
	// temp[3] = servo4.temp();
	// temp[4] = servo5.temp();
	// temp[5] = servo6.temp();
	// delay(1000);


	// =====================================================================================================
    // ROS SETUP
    // =====================================================================================================

	allocator = rcl_get_default_allocator();									// Initialize micro-ROS allocator
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));					// Initialize support options
	
	// create node
  	RCCHECK(rclc_node_init_default(&node_hiwonder, "Hiwonder_xArm_node", "", &support));

	// =====================================================================================================
    // SERVICES
    // =====================================================================================================


	// =====================================================================================================
    // PUBLISHERS
    // =====================================================================================================

	// Servo position publisher (publishes at a rate of 100 Hz)
	RCCHECK(rclc_publisher_init_default(
		&publisher_servo_pos, 
		&node_hiwonder, 
		ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, JointState), 
		"servo_pos_publisher"));


	// =====================================================================================================
    // SUBSCRIBERS
    // =====================================================================================================

	// Subscriber for moving a single servo
	// RCCHECK(rclc_subscription_init_default(
	// 	&subscriber_one_servo_cmd, 
	// 	&node_hiwonder, 
	// 	ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int64MultiArray), 
	// 	"single_servo_cmd_sub"));

	// Subscriber for moving multiple servos
	RCCHECK(rclc_subscription_init_default(
		&subscriber_multi_servo_cmd, 
		&node_hiwonder, 
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int64MultiArray), 
		"multi_servo_cmd_sub"));


	// =====================================================================================================
    // TIMERS
    // =====================================================================================================


	// =====================================================================================================
    // MESSAGE CONSTRUCTION
    // =====================================================================================================

	// (1) servo_position_array_msg
	// Assigning dynamic memory to the name string sequence (this is not working)

	bool success = rosidl_runtime_c__String__Sequence__init(&name__string_sequence, 6);
	servo_position_array_msg.name = name__string_sequence;
	success = rosidl_runtime_c__String__assignn(&servo_position_array_msg.name.data[0], "Servo1", 6);
	success = rosidl_runtime_c__String__assignn(&servo_position_array_msg.name.data[1], "Servo2", 6);
	success = rosidl_runtime_c__String__assignn(&servo_position_array_msg.name.data[2], "Servo3", 6);
	success = rosidl_runtime_c__String__assignn(&servo_position_array_msg.name.data[3], "Servo4", 6);
	success = rosidl_runtime_c__String__assignn(&servo_position_array_msg.name.data[4], "Servo5", 6);
	success = rosidl_runtime_c__String__assignn(&servo_position_array_msg.name.data[5], "Servo6", 6);


	// Assigning dynamic memory to POSITION rosidl_runtime_c__double__Sequence 
	servo_position_array_msg.position.capacity = 6;
	servo_position_array_msg.position.size = 6;
	servo_position_array_msg.position.data = (double*) malloc(servo_position_array_msg.position.capacity * sizeof(double));
	servo_position_array_msg.position.data = pos; 

	// Assigning dynamic memory to EFFORT rosidl_runtime_c__double__Sequence 
	servo_position_array_msg.effort.capacity = 6;
	servo_position_array_msg.effort.size = 6;
	servo_position_array_msg.effort.data = (double*) malloc(servo_position_array_msg.position.capacity * sizeof(double));
	servo_position_array_msg.effort.data = effort_array;

	// Assigning dynamic memory to EFFORT rosidl_runtime_c__double__Sequence 
	servo_position_array_msg.velocity.capacity = 6;
	servo_position_array_msg.velocity.size = 6;
	servo_position_array_msg.velocity.data = (double*) malloc(servo_position_array_msg.position.capacity * sizeof(double));
	servo_position_array_msg.velocity.data = vel;

	// header
	// Assigning dynamic memory to the frame_id char sequence
	servo_position_array_msg.header.frame_id.capacity = 100;
	servo_position_array_msg.header.frame_id.data = (char*) malloc(servo_position_array_msg.header.frame_id.capacity * sizeof(char));
	servo_position_array_msg.header.frame_id.size = 0;

	// Assigning value to the frame_id char sequence
	strcpy(servo_position_array_msg.header.frame_id.data, "frame id");
	servo_position_array_msg.header.frame_id.size = strlen(servo_position_array_msg.header.frame_id.data);

	// Assigning time stamp
	servo_position_array_msg.header.stamp.sec = (uint16_t)(rmw_uros_epoch_millis()/1000);
	servo_position_array_msg.header.stamp.nanosec = (uint32_t)rmw_uros_epoch_nanos();
	

	// // (2) one_servo_cmd_msg
	// single_servo_cmd_msg_in.data.capacity = 3; 
  	// single_servo_cmd_msg_in.data.size = 3;
  	// single_servo_cmd_msg_in.data.data = (int64_t*) malloc(3 * sizeof(int64_t));

	// single_servo_cmd_msg_in.layout.dim.capacity = 3;
	// single_servo_cmd_msg_in.layout.dim.size = 3;
	// single_servo_cmd_msg_in.layout.dim.data = (std_msgs__msg__MultiArrayDimension*) malloc(3 * sizeof(std_msgs__msg__MultiArrayDimension));

	// for(size_t i = 0; i < single_servo_cmd_msg_in.layout.dim.capacity; i++){
	// 	single_servo_cmd_msg_in.layout.dim.data[i].label.capacity = 10;
	// 	single_servo_cmd_msg_in.layout.dim.data[i].label.size = 10;
	// 	single_servo_cmd_msg_in.layout.dim.data[i].label.data = (char*) malloc(single_servo_cmd_msg_in.layout.dim.data[i].label.capacity * sizeof(char));
	// }


	// (3) multi_servo_cmd_msg
	multi_servo_cmd_msg_in.data.capacity = 12;
	multi_servo_cmd_msg_in.data.size = 0;
  	multi_servo_cmd_msg_in.data.data = (int64_t*) malloc(single_servo_cmd_msg_in.data.capacity * sizeof(int64_t));

	multi_servo_cmd_msg_in.layout.dim.capacity = 3;
	multi_servo_cmd_msg_in.layout.dim.size = 0;
	multi_servo_cmd_msg_in.layout.dim.data = (std_msgs__msg__MultiArrayDimension*) malloc(multi_servo_cmd_msg_in.layout.dim.capacity * sizeof(std_msgs__msg__MultiArrayDimension));

	for(size_t i = 0; i < multi_servo_cmd_msg_in.layout.dim.capacity; i++){
		multi_servo_cmd_msg_in.layout.dim.data[i].label.capacity = 10;
		multi_servo_cmd_msg_in.layout.dim.data[i].label.size = 0;
		multi_servo_cmd_msg_in.layout.dim.data[i].label.data = (char*) malloc(multi_servo_cmd_msg_in.layout.dim.data[i].label.capacity * sizeof(char));
	}


	// =====================================================================================================
    // EXECUTORS
    // =====================================================================================================

	// init executors
	RCCHECK(rclc_executor_init(&executor_servo_pos_publish, &support.context, 1, &allocator));			// executor for publishing servo pos.
	// RCCHECK(rclc_executor_init(&executor_single_servo_cmd_sub, &support.context, 1, &allocator));		// executor for one servo cmd subscriber
	RCCHECK(rclc_executor_init(&executor_multi_servo_move_cmd_sub, &support.context, 1, &allocator));	// executor for multi servo cmd subscriber

	// executor additions
	// RCCHECK(rclc_executor_add_subscription(&executor_single_servo_cmd_sub, &subscriber_one_servo_cmd, &single_servo_cmd_msg_in, &subscription_callback_single_servo, ON_NEW_DATA));
	RCCHECK(rclc_executor_add_subscription(&executor_multi_servo_move_cmd_sub, &subscriber_multi_servo_cmd, &multi_servo_cmd_msg_in, &subscription_callback_multi_servo, ON_NEW_DATA));
}


// =====================================================================================================
// LOOP
// =====================================================================================================

	
void loop() {	
	if (publish_servo_pos_flag == true) {
		pos[0] = servo1.pos_read();
		pos[1] = servo2.pos_read();
		pos[2] = servo3.pos_read();
		pos[3] = servo4.pos_read();
		pos[4] = servo5.pos_read();
		pos[5] = servo6.pos_read();
		delay(10);

		effort_array[0]++;


		// Update servo_position_array_msg
		servo_position_array_msg.position.data = pos;
		servo_position_array_msg.effort.data = effort_array;
		servo_position_array_msg.velocity.data = vel;
		servo_position_array_msg.header.stamp.sec = (uint16_t)(rmw_uros_epoch_millis()/1000); 	// timestamp
		servo_position_array_msg.header.stamp.nanosec = (uint32_t)rmw_uros_epoch_nanos();		// timestamp

		
		// Publishes
		RCSOFTCHECK(rcl_publish(&publisher_servo_pos, &servo_position_array_msg, NULL));
		publish_servo_pos_flag = false;
	}

	delay(5);

	// Subscribers added to spin
	// RCCHECK(rclc_executor_spin_some(&executor_single_servo_cmd_sub, RCL_MS_TO_NS(100)));
	RCCHECK(rclc_executor_spin_some(&executor_multi_servo_move_cmd_sub, RCL_MS_TO_NS(100)));

}



// ===================================================================================================== //
//																										 //
// 											END IT ALL													 // 
//																										 //
// ===================================================================================================== //

// #include <micro_ros_platformio.h>
// // #include <example_interfaces/srv/add_two_ints.h>
// #include <stdio.h>
// #include <rcl/error_handling.h>
// #include <rclc/rclc.h>
// #include <rclc/executor.h>
// #include <std_msgs/msg/int64.h>

// rcl_node_t node;
// rclc_support_t support;
// rcl_allocator_t allocator;
// rclc_executor_t executor;

// rcl_service_t service;
// rcl_wait_set_t wait_set;

// // example_interfaces__srv__AddTwoInts_Response res;
// // example_interfaces__srv__AddTwoInts_Request req;

// #define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){while(1){};}}
// #define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

// // void service_callback(const void * req, void * res){
// //   example_interfaces__srv__AddTwoInts_Request * req_in = (example_interfaces__srv__AddTwoInts_Request *) req;
// //   example_interfaces__srv__AddTwoInts_Response * res_in = (example_interfaces__srv__AddTwoInts_Response *) res;

// //   //printf("Service request value: %d + %d.\n", (int) req_in->a, (int) req_in->b);

// //   res_in->sum = req_in->a + req_in->b;
// // }

// void setup() {

// }


// void loop() {
//   delay(100);
//   RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));
// }
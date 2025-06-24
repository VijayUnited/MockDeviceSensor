#include <iostream>
#include <thread>
#include <queue>
#include <condition_variable>
#include <random>
#include <iomanip>
#include <string.h>
#include <unistd.h>
#include <poll.h>

/**********************************************************************************************/
//
// Thread Safe supporting class
//
/**********************************************************************************************/
template <typename T> 
class ThreadSafeQueue
{
    public:
    void push(const T& item)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push(item);
        m_condition.notify_one(); //Notify one waiting consumer
    }

    T pop() 
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [this]{return !m_queue.empty();}); //Wait until queue is not empty
        T item = m_queue.front();
        m_queue.pop();
        return item;
    }

    bool empty() const 
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    private:
    std::queue<T> m_queue;
    mutable std::mutex m_mutex; // mutable for const methods like empty()
    std::condition_variable m_condition;
};

/**********************************************************************************************/
//
// Controller Base Class
//
/**********************************************************************************************/
struct SensorData
{
    double value;
    char timestamp[sizeof("hh:mm:ss")];
};

class Controller
{
    public:
    virtual void init_controller() = 0;
    virtual double read_controller() = 0;
    virtual void write_controller(int) = 0;
    virtual void reset_controller() = 0;
    virtual ~Controller() = default; //Virtual distructor

    ThreadSafeQueue<SensorData> SensorQueue;
};

/**********************************************************************************************/
//
// Derived Classes
//
/**********************************************************************************************/
class TemperatureSensorController : public Controller 
{

    public:
    void init_controller() override 
    {
        // Specific initialization for temperature sensor
        temperature = 0;
        // MOCK : skip hardware init.
        std::cout << "Temperature Sensor Controller initialized." << std::endl;
    }

    void reset_controller() override
    {
        // Specific reset sequence for temperature sensor
        // MOCK : skip hardware reset.
        std::cout << "Temperature Sensor Controller reset." << std::endl;
    }

    double read_controller() override 
    {
        // Logic for read the temperature sensor
        // MOCK : generate random value
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> temp_dist(35.0, 45.0);
        temperature = static_cast<float>(temp_dist(gen));

        time_t now = time(0);
        struct tm *local = localtime(&now);
        char buff[sizeof("hh:mm:ss")];
        strftime(buff, sizeof(buff), "%H:%M:%S", local);

        SensorData data;
        data.value = temperature;
        strncpy(data.timestamp, buff, sizeof("hh:mm:ss"));
        SensorQueue.push(data);
        return temperature;
    }

    void write_controller(int degree) override
    {
        // Logic to set the controller value
        // MOCK : skip hardware write
    }

    private:
    float temperature;
};

class MotorController : public Controller 
{
    public:
    void init_controller() override 
    {
        // Specific initialization for motor controller
        motor_rpm = 2000;
        // MOCK : skip hardware init.
        std::cout << "Motor Controller initialized." << std::endl;
    }

    void reset_controller() override
    {
        // Specific reset sequence for Motor Controller
        // MOCK : skip hardware reset.
        std::cout << "Motor Controller reset." << std::endl;
    }

    double read_controller() override 
    {
        // Logic for read motor controller sensor
        // MOCK : return current motor_rpm value.
        time_t now = time(0);
        struct tm *local = localtime(&now);
        char buff[sizeof("hh:mm:ss")];
        strftime(buff, sizeof(buff), "%H:%M:%S", local);

        SensorData data;
        data.value = (double)motor_rpm; 
        strncpy(data.timestamp, buff, sizeof("hh:mm:ss"));

        SensorQueue.push(data);
        return (double)motor_rpm;
    }

    void setSpeed(int rpm)
    {
       write_controller(rpm);
    }

    void write_controller(int rpm) override
    {
       // Logic to set the RPM value
       motor_rpm = rpm; //MOCK settings.
    }

    private:
    int motor_rpm;
};

/**********************************************************************************************/
//
// Thread Functions
//
/**********************************************************************************************/
// Global mutex for synchronized output
std::mutex g_cout_mutex;
// Global parameter to exit all the threads
static int exit_program = false;

// Function to handle CLI input
void cli_handler(Controller* objPtr, Controller* objPtr2) 
{
    std::string command;
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    {
        std::lock_guard<std::mutex> lock(g_cout_mutex);
        std::cout << "Sensor display started. Enter 'x' and enter to quit whole program: \n";
    }

    while (true) 
    {
        int ret = poll(&pfd, 1, 1000);  // timeout of 1000ms
        if(ret == 1) // there is something to read
        {
            std::getline(std::cin, command);
            if (command == "x") 
            {
                exit_program = true;
                break;
            } 
            else if (command == "info") 
            {
                std::lock_guard<std::mutex> lock(g_cout_mutex);
                std::cout << "CLI is active. Type 'x' and enter to terminate. \n" << std::endl;
            } 
        }
        else
        {
            SensorData sensor_1 = objPtr->SensorQueue.pop();
            SensorData sensor_2 = objPtr2->SensorQueue.pop();

            std::lock_guard<std::mutex> lock(g_cout_mutex);
            std::cout << std::fixed;
            std::cout << "[Time: " << sensor_1.timestamp << "] " << "Temperature " << std::setprecision(1) << sensor_1.value << "C | " << "[Time: " << sensor_2.timestamp << "] " << "Motor Speed: " << std::setprecision(0) << sensor_2.value << " RPM" << std::endl;
        }
    }
}

// Function to read temperature sensor controller at every 200msec
void read_temp_sensor(Controller* objPtr)
{
    while (exit_program == false)
    {
        int sensor_value = objPtr->read_controller();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// Function to read motor controller at every 500msec
void read_motor_sensor(Controller* objPtr)
{
    while (exit_program == false)
    {
        int sensor_value = objPtr->read_controller();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}


/**********************************************************************************************/
//
// main function
//
/**********************************************************************************************/
int main() 
{
    Controller* pTempSensorController = new TemperatureSensorController();
    Controller* pMotorController = new MotorController();

    pTempSensorController->init_controller();
    pMotorController->init_controller();

    // Create threads for sensor reading
    std::thread tempsensor_thread(read_temp_sensor, pTempSensorController);
    std::thread motorsensor_thread(read_motor_sensor, pMotorController);

    // Create a thread for the command-line interface
    std::thread cli_thread(cli_handler, pTempSensorController, pMotorController);

    tempsensor_thread.join();
    motorsensor_thread.join();

    // Wait for the CLI thread to finish (when 'x' is typed)
    cli_thread.join();

    delete pTempSensorController;
    delete pMotorController;
    std::cout << "Program terminated." << std::endl;

    return 0;
}
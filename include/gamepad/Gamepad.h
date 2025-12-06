#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <string>
#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include "GamepadStatus.h"

class Gamepad
{
public:
    /**
     *  @brief Initializes a Gamepad object.
     *  @param path The path to the 'jsX' input file stream as a string
     *  @return The created Gamepad object
     */
    Gamepad(std::string path);

    /**
     *  @brief Initializes a Gamepad object.
     *  @param index The index of the joystick to be initialized
     *  @return The created Gamepad object
     */
    Gamepad(int index);

    /**
     *  @brief Destructor for Gamepad object, ensures cleanup
     */
    ~Gamepad();

    /**
     *  @brief Updates the state of the Gamepad, including errors.
     *  @details If you wish to interact with the Gamepad iteratively or periodically,
     *  @details call this before any logic.
     */
    void refresh();

    /**
     *  @brief Gives the most up-to-date value of the given axis.
     *  @param index The index of the desired axis
     *  @return The current value of the axis
     */
    short getAxis(int index);

    /**
     *  @brief Gives the most up-to-date value of the given button.
     *  @param index The index of the desired button
     *  @return The current value of the button
     */
    short getButton(int index);

    /**
     *  @brief Gets current status of Gamepad.
     *  @returns Current GamepadStatus enum value
     */
    GamepadStatus getStatus();

    /**
     *  @brief Checks if the Gamepad is in an error state (doesn't exist or failed to open/read)
     *  @return true if in error state, false otherwise
     */
    bool getErr();

    /**
     * @brief Gets value for device file path. Locks mutex.
     * @returns A string for the new device file path
     */
    std::string getPath();

    /**
     *  @brief Opens the stream of a given path to a 'jsX' file.
     *  @param path The path to the file as a string
     *  @return The file descriptor of the stream
     *  @details If the operation was successful, return positive integer
     *  @details Otherwise, set device error state and output error to console.
     */
    int openStream(std::string path);

    /**
     *  @brief Closes the file stream. Sets device to error state.
     *  @return The output of the close() system call
     */
    int closeStream();

private:
    std::string path;
    std::mutex pathMutex;
    std::atomic<int> fd;   
    std::atomic<bool> reconnecting;
    std::thread reconnectionThread;
    GamepadStatus status;
    std::array<short, 6> axes{};
    std::array<short, 15> buttons{};

    /**
     * @brief Set a new value for device file path. Locks mutex.
     * @param newPath The new string for path
     */
    void setPath(std::string newPath);

    /**
     *  @brief Updates the status based on the current 'errno' status,
     *  @brief see: https://man7.org/linux/man-pages/man2/read.2.html
     */
    void updateStatus();

    /**
     *  @brief Attempts to reopen file stream.
     *  @returns The file descriptor of the stream
     */
    int reconnect();

    /**
     *  @brief Asynchronously attempt reconnection every ~250ms.
     */
    void startReconnectionThread();
};

#endif // GAMEPAD_H
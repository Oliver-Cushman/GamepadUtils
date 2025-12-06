#include "../include/gamepad/Gamepad.h"
#include "../include/gamepad/JSEvent.h"

#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <chrono>

/**
 *  @brief Initializes a Gamepad object.
 *  @param path The path to the 'jsX' input file stream as a string
 *  @return The created Gamepad object
 */
Gamepad::Gamepad(std::string path)
{
    this->reconnecting.store(false);
    this->openStream(path);
    this->refresh();
}

/**
 *  @brief Initializes a Gamepad object.
 *  @param index The index of the joystick to be initialized
 *  @return The created Gamepad object
 */
Gamepad::Gamepad(int index) : Gamepad(std::string("/dev/input/js") + char('0' + index)) {}

/**
 *  @brief Destructor for Gamepad object, ensures cleanup
 */
Gamepad::~Gamepad()
{
    this->closeStream();
}

/**
 *  @brief Updates the state of the Gamepad, including errors.
 *  @details If you wish to interact with the Gamepad iteratively or periodically,
 *  @details call this before any logic.
 */
void Gamepad::refresh()
{
    ssize_t bytesRead = -1;
    JSEvent event;
    // Read sizeof(JSEvent) bytes into event
    // read() updates errno to check status
    while ((bytesRead = read(this->fd.load(), &event, sizeof(JSEvent))) > 0)
    {
        if (event.type == 1)
            this->buttons[event.number] = event.value;
        else if (event.type == 2)
            this->axes[event.number] = event.value;
    }
    this->updateStatus();
    if (this->getErr() && !this->reconnecting.load())
    {
        this->reconnecting.store(true);
        this->startReconnectionThread();
    }
}

/**
 *  @brief Gives the most up-to-date value of the given axis.
 *  @param index The index of the desired axis
 *  @return The current value of the axis
 */
short Gamepad::getAxis(int index)
{
    // Invalid index
    if (index >= this->axes.size() || index < 0)
        return 0;
    return this->axes[index];
}

/**
 *  @brief Gives the most up-to-date value of the given button.
 *  @param index The index of the desired button
 *  @return The current value of the button
 */
short Gamepad::getButton(int index)
{
    // Invalid index
    if (index >= this->buttons.size() || index < 0)
        return 0;
    return this->buttons[index];
}

/**
 *  @brief Gets current status of Gamepad.
 *  @returns Current GamepadStatus enum value
 */
GamepadStatus Gamepad::getStatus()
{
    return this->status;
}

/**
 *  @brief Checks if the Gamepad is in an error state (doesn't exist or failed to open/read)
 *  @return true if in error state, false otherwise
 */
bool Gamepad::getErr()
{
    return this->status < GamepadStatus::OK;
}

/**
 * @brief Gets value for device file path. Locks mutex.
 * @returns A string for the new device file path
 */
std::string Gamepad::getPath()
{
    std::lock_guard<std::mutex> lock(pathMutex);
    return this->path;
}

/**
 *  @brief Opens the stream of a given path to a 'jsX' file.
 *  @param path The path to the file as a string
 *  @return The file descriptor of the stream
 *  @details If the operation was successful, return positive integer
 *  @details Otherwise, set device error state and output error to console.
 */
int Gamepad::openStream(std::string path)
{
    this->reconnecting.store(false);
    this->setPath(path);
    this->fd.store(open(path.c_str(), O_RDONLY | O_NONBLOCK));
    return this->fd.load();
}

/**
 *  @brief Closes the file stream. Sets device to error state.
 *  @return The output of the close() system call
 */
int Gamepad::closeStream()
{
    this->reconnecting.store(false);
    return close(this->fd.load());
}

/**
 * @brief Set a new value for device file path. Locks mutex.
 * @param newPath The new string for path
 */
void Gamepad::setPath(std::string newPath)
{
    std::lock_guard<std::mutex> lock(pathMutex);
    this->path = newPath;
}

/**
 *  @brief Updates the status based on the current 'errno' status,
 *  @brief see: https://man7.org/linux/man-pages/man2/read.2.html
 */
void Gamepad::updateStatus()
{
    switch (errno)
    {
    case EBADF:
        // Invalid file descriptor
        this->status = GamepadStatus::INVALID_FILE_ERROR;
        break;
    case EINVAL:
        // Invalid file object
        this->status = GamepadStatus::INVALID_FILE_ERROR;
        break;
    case EIO:
        // I/O error
        this->status = GamepadStatus::IO_ERROR;
        break;
    case EAGAIN:
        // Read would block but was cancelled with O_NONBLOCK flag
        this->status = GamepadStatus::OK;
        break;
    default:
        // Unknown error
        this->status = GamepadStatus::ERROR;
        break;
    }
}

/**
 *  @brief Attempts to reopen file stream.
 *  @returns The file descriptor of the stream
 */
int Gamepad::reconnect()
{
    this->fd.store(open(this->getPath().c_str(), O_RDONLY | O_NONBLOCK));
    return this->fd.load();
}

/**
 *  @brief Asynchronously attempt reconnection every ~250ms.
 */
void Gamepad::startReconnectionThread()
{
    std::thread([this]()
                {
            while (this->reconnect() < 0 && this->reconnecting.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            this->reconnecting.store(false); })
        .detach();
}
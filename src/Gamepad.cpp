#include "../include/gamepad/Gamepad.h"
#include "../include/gamepad/JSEvent.h"

#include <fcntl.h>
#include <unistd.h>
#include <chrono>

/**
 *  @brief Initializes a Gamepad object.
 *  @param path The path to the 'jsX' input file stream as a string
 *  @return The created Gamepad object
 */
Gamepad::Gamepad(const std::string &path)
{
    this->fd = -1;
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
    if (!this->reconnecting.load())
        // Connection successful, stop background thread
        stopReconnectionThread();
    else
        // Still disconnected, no point in refreshing state
        return;

    ssize_t bytesRead = -1;
    JSEvent event;
    // Read sizeof(JSEvent) bytes into event
    // read() updates errno to check status
    while ((bytesRead = this->safeRead(&event, sizeof(JSEvent))) > 0)
    {
        if (event.type == 1)
            this->buttons[event.number] = event.value;
        else if (event.type == 2)
            this->axes[event.number] = event.value;
    }
    int err = errno;
    // Check for read errors
    this->updateStatus(err);
    if (this->getErr())
    {
        // Problem with controller, attempt reconnection
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
 *  @brief Opens the stream of a given path to a 'jsX' file.
 *  @param path The path to the file as a string
 *  @return The file descriptor of the stream
 *  @details If the operation was successful, return positive integer
 *  @details Otherwise, set device error state and output error to console.
 */
int Gamepad::openStream(const std::string &path)
{
    // Switching / opening new stream, stop background thread
    this->stopReconnectionThread();
    this->path = path;
    return this->safeOpen(path);
}

/**
 *  @brief Closes the file stream. Sets device to error state.
 *  @return The output of the close() system call
 */
int Gamepad::closeStream()
{
    // Stopping device file stream, stop background thread
    this->stopReconnectionThread();
    return this->safeClose();
}

/**
 *  @brief Updates the status based on the current 'errno' status,
 *  @brief see: https://man7.org/linux/man-pages/man2/read.2.html
 *  @param err The error value from errno
 */
void Gamepad::updateStatus(int err)
{
    switch (err)
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
 *  @brief Asynchronously attempt reconnection every ~250ms.
 */
void Gamepad::startReconnectionThread()
{
    if (this->reconnecting.exchange(true))
        return;
    std::string path = this->path;
    this->reconnectionThread = std::thread([this, path]()
                                           {
        while (this->safeOpen(path) < 0 && this->reconnecting.load()) 
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        this->reconnecting.store(false); });
}

/**
 * @brief Stop reconnecton thread.
 */
void Gamepad::stopReconnectionThread()
{
    this->reconnecting.store(false);
    if (this->reconnectionThread.joinable())
    {
        this->reconnectionThread.join();
    }
}

/**
 *  @brief Provides a safe mutex lock around the read syscall.
 *  @param buf A reference to a buffer to read into
 *  @param size The size of the buffer / how many bytes to read
 *  @return The amount of bytes read as a ssize_t
 */
ssize_t Gamepad::safeRead(void *buf, size_t size)
{
    std::lock_guard<std::mutex> lock(this->fdMutex);
    return read(this->fd, buf, size);
}

/**
 *  @brief Provides a safe mutex lock around the open syscall while closing already open file descriptors.
 *  @param path A string of the file path to the device
 *  @return The new file descriptor
 */
int Gamepad::safeOpen(const std::string &path)
{
    int newFd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    std::lock_guard<std::mutex> lock(this->fdMutex);
    int oldFd = this->fd;
    this->fd = newFd;
    if (oldFd >= 0)
        close(oldFd);
    return newFd;
}

/**
 *  @brief Provides a safe mutex lock around the close syscall
 *  @return An int representing the outcome of close()
 */
int Gamepad::safeClose()
{
    std::lock_guard<std::mutex> lock(this->fdMutex);
    int oldFd = this->fd;
    this->fd = -1;
    if (oldFd >= 0)
        return close(oldFd);
    return 0;
}
#include "core/remote/holepunch.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <chiaki/remote/holepunch.h>

namespace py = pybind11;

class NotificationWrapper
{
public:
    Notification notification;
    std::shared_ptr<NotificationWrapper> next;

    NotificationWrapper() : notification{} {}

    // Handle JSON as string (since json_object is an opaque type)
    std::string get_json() const
    {
        return notification.json ? json_object_to_json_string(notification.json) : "";
    }

    void set_json(const std::string &json_str)
    {
        if (notification.json)
        {
            json_object_put(notification.json); // Free existing object
        }
        notification.json = json_tokener_parse(json_str.c_str());
    }

    std::string get_json_buf() const
    {
        return notification.json_buf ? std::string(notification.json_buf) : "";
    }

    void set_json_buf(const std::string &buf)
    {
        notification.json_buf = strdup(buf.c_str());
        notification.json_buf_size = buf.size();
    }

    size_t get_json_buf_size() const { return notification.json_buf_size; }

    NotificationType get_type() const { return notification.type; }
    void set_type(NotificationType value) { notification.type = value; }

    std::shared_ptr<NotificationWrapper> get_next() const { return next; }
    void set_next(std::shared_ptr<NotificationWrapper> next_notification) { next = next_notification; }

    std::string to_string() const
    {
        return "<Notification type=" + std::to_string(notification.type) +
               " json=" + get_json() + ">";
    }
};

class NotificationQueueWrapper
{
private:
    NotificationQueue queue;

public:
    NotificationQueueWrapper() : queue{nullptr, nullptr} {}

    void enqueue(std::shared_ptr<NotificationWrapper> notif)
    {
        Notification *new_notif = new Notification(*notif); // Copy data
        new_notif->next = nullptr;

        if (!queue.rear)
        {
            queue.front = queue.rear = new_notif;
        }
        else
        {
            queue.rear->next = new_notif;
            queue.rear = new_notif;
        }
    }

    std::shared_ptr<NotificationWrapper> dequeue()
    {
        if (!queue.front)
            return nullptr;

        Notification *temp = queue.front;
        queue.front = queue.front->next;
        if (!queue.front)
            queue.rear = nullptr;

        auto result = std::make_shared<NotificationWrapper>();
        result->set_type(temp->type);
        result->set_json(temp->json ? json_object_to_json_string(temp->json) : "");
        result->set_json_buf(temp->json_buf ? std::string(temp->json_buf) : "");

        delete temp; // Free memory
        return result;
    }

    bool is_empty() const
    {
        return queue.front == nullptr;
    }

    std::string to_string() const
    {
        int count = 0;
        Notification *temp = queue.front;
        while (temp)
        {
            count++;
            temp = temp->next;
        }
        return "<NotificationQueue size=" + std::to_string(count) + ">";
    }
};

class SessionWrapper
{
public:
    Session session;

    SessionWrapper() : session{} {}

    // String fields
    std::string get_oauth_header() const { return session.oauth_header ? std::string(session.oauth_header) : ""; }
    void set_oauth_header(const std::string &value) { session.oauth_header = strdup(value.c_str()); }

    std::string get_session_id_header() const { return session.session_id_header ? std::string(session.session_id_header) : ""; }
    void set_session_id_header(const std::string &value) { session.session_id_header = strdup(value.c_str()); }

    std::string get_online_id() const { return session.online_id ? std::string(session.online_id) : ""; }
    void set_online_id(const std::string &value) { session.online_id = strdup(value.c_str()); }

    // Fixed-size arrays
    std::vector<uint8_t> get_console_uid() const { return std::vector<uint8_t>(session.console_uid, session.console_uid + 32); }
    void set_console_uid(const std::vector<uint8_t> &value)
    {
        if (value.size() != 32)
            throw std::runtime_error("console_uid must be 32 bytes");
        memcpy(session.console_uid, value.data(), 32);
    }

    std::vector<uint8_t> get_hashed_id_local() const { return std::vector<uint8_t>(session.hashed_id_local, session.hashed_id_local + 20); }
    void set_hashed_id_local(const std::vector<uint8_t> &value)
    {
        if (value.size() != 20)
            throw std::runtime_error("hashed_id_local must be 20 bytes");
        memcpy(session.hashed_id_local, value.data(), 20);
    }

    std::vector<uint8_t> get_hashed_id_console() const { return std::vector<uint8_t>(session.hashed_id_console, session.hashed_id_console + 20); }
    void set_hashed_id_console(const std::vector<uint8_t> &value)
    {
        if (value.size() != 20)
            throw std::runtime_error("hashed_id_console must be 20 bytes");
        memcpy(session.hashed_id_console, value.data(), 20);
    }

    // Networking fields
    std::shared_ptr<ChiakiSocket> get_sock() const { return std::make_shared<ChiakiSocket>(session.sock); }
    void set_sock(std::shared_ptr<ChiakiSocket> sock) { session.sock = sock->get_socket(); }

    std::shared_ptr<ChiakiSocket> get_ctrl_sock() const { return std::make_shared<ChiakiSocket>(session.ctrl_sock); }
    void set_ctrl_sock(std::shared_ptr<ChiakiSocket> sock) { session.ctrl_sock = sock->get_socket(); }

    std::shared_ptr<ChiakiSocket> get_data_sock() const { return std::make_shared<ChiakiSocket>(session.data_sock); }
    void set_data_sock(std::shared_ptr<ChiakiSocket> sock) { session.data_sock = sock->get_socket(); }

    // Integer fields
    uint64_t get_account_id() const { return session.account_id; }
    void set_account_id(uint64_t value) { session.account_id = value; }

    uint16_t get_sid_local() const { return session.sid_local; }
    void set_sid_local(uint16_t value) { session.sid_local = value; }

    uint16_t get_sid_console() const { return session.sid_console; }
    void set_sid_console(uint16_t value) { session.sid_console = value; }

    uint16_t get_local_port_ctrl() const { return session.local_port_ctrl; }
    void set_local_port_ctrl(uint16_t value) { session.local_port_ctrl = value; }

    uint16_t get_local_port_data() const { return session.local_port_data; }
    void set_local_port_data(uint16_t value) { session.local_port_data = value; }

    bool get_stun_random_allocation() const { return session.stun_random_allocation; }
    void set_stun_random_allocation(bool value) { session.stun_random_allocation = value; }

    bool get_ws_thread_should_stop() const { return session.ws_thread_should_stop; }
    void set_ws_thread_should_stop(bool value) { session.ws_thread_should_stop = value; }

    bool get_ws_open() const { return session.ws_open; }
    void set_ws_open(bool value) { session.ws_open = value; }

    bool get_main_should_stop() const { return session.main_should_stop; }
    void set_main_should_stop(bool value) { session.main_should_stop = value; }

    std::string to_string() const
    {
        return "<SessionWrapper oauth_header=" + get_oauth_header() +
               " session_id_header=" + get_session_id_header() +
               " account_id=" + std::to_string(session.account_id) +
               " online_id=" + get_online_id() +
               " ctrl_port=" + std::to_string(session.ctrl_port) +
               " client_local_ip=" + std::string(session.client_local_ip) +
               ">";
    }
};

void init_core_remote_holepunch(py::module &m)
{
    py::enum_<NotificationType>(m, "NotificationType")
        .value("UNKNOWN", NOTIFICATION_TYPE_UNKNOWN)
        .value("SESSION_CREATED", NOTIFICATION_TYPE_SESSION_CREATED)
        .value("MEMBER_CREATED", NOTIFICATION_TYPE_MEMBER_CREATED)
        .value("MEMBER_DELETED", NOTIFICATION_TYPE_MEMBER_DELETED)
        .value("CUSTOM_DATA1_UPDATED", NOTIFICATION_TYPE_CUSTOM_DATA1_UPDATED)
        .value("SESSION_MESSAGE_CREATED", NOTIFICATION_TYPE_SESSION_MESSAGE_CREATED)
        .value("SESSION_DELETED", NOTIFICATION_TYPE_SESSION_DELETED)
        .export_values();

    py::class_<NotificationWrapper>(m, "Notification")
        .def(py::init<>())
        .def_property("type", &NotificationWrapper::get_type, &NotificationWrapper::set_type)
        .def_property("json", &NotificationWrapper::get_json, &NotificationWrapper::set_json)
        .def_property("json_buf", &NotificationWrapper::get_json_buf, &NotificationWrapper::set_json_buf)
        .def_property("json_buf_size", &NotificationWrapper::get_json_buf_size, nullptr)
        .def_property("next", &NotificationWrapper::get_next, &NotificationWrapper::set_next)
        .def("__repr__", &NotificationWrapper::to_string);

    py::class_<NotificationQueueWrapper>(m, "NotificationQueue")
        .def(py::init<>())
        .def("enqueue", &NotificationQueueWrapper::enqueue)
        .def("dequeue", &NotificationQueueWrapper::dequeue)
        .def("is_empty", &NotificationQueueWrapper::is_empty)
        .def("__repr__", &NotificationQueueWrapper::to_string);

    py::enum_<SessionState>(m, "SessionState")
        .value("INIT", SESSION_STATE_INIT)
        .value("WS_OPEN", SESSION_STATE_WS_OPEN)
        .value("CREATED", SESSION_STATE_CREATED)
        .value("STARTED", SESSION_STATE_STARTED)
        .value("CLIENT_JOINED", SESSION_STATE_CLIENT_JOINED)
        .value("DATA_SENT", SESSION_STATE_DATA_SENT)
        .value("CONSOLE_JOINED", SESSION_STATE_CONSOLE_JOINED)
        .value("CUSTOMDATA1_RECEIVED", SESSION_STATE_CUSTOMDATA1_RECEIVED)
        .value("CTRL_OFFER_RECEIVED", SESSION_STATE_CTRL_OFFER_RECEIVED)
        .value("CTRL_OFFER_SENT", SESSION_STATE_CTRL_OFFER_SENT)
        .value("CTRL_CONSOLE_ACCEPTED", SESSION_STATE_CTRL_CONSOLE_ACCEPTED)
        .value("CTRL_CLIENT_ACCEPTED", SESSION_STATE_CTRL_CLIENT_ACCEPTED)
        .value("CTRL_ESTABLISHED", SESSION_STATE_CTRL_ESTABLISHED)
        .value("DATA_OFFER_RECEIVED", SESSION_STATE_DATA_OFFER_RECEIVED)
        .value("DATA_OFFER_SENT", SESSION_STATE_DATA_OFFER_SENT)
        .value("DATA_CONSOLE_ACCEPTED", SESSION_STATE_DATA_CONSOLE_ACCEPTED)
        .value("DATA_CLIENT_ACCEPTED", SESSION_STATE_DATA_CLIENT_ACCEPTED)
        .value("DATA_ESTABLISHED", SESSION_STATE_DATA_ESTABLISHED)
        .value("DELETED", SESSION_STATE_DELETED)
        .export_values();

    py::enum_<ChiakiHolepunchConsoleType>(m, "HolepunchConsoleType")
        .value("PS4", CHIAKI_HOLEPUNCH_CONSOLE_TYPE_PS4)
        .value("PS5", CHIAKI_HOLEPUNCH_CONSOLE_TYPE_PS5)
        .export_values();

    py::class_<SessionWrapper>(m, "Session")
        .def(py::init<>())
        .def_property("oauth_header", &SessionWrapper::get_oauth_header, &SessionWrapper::set_oauth_header)
        .def_property("session_id_header", &SessionWrapper::get_session_id_header, &SessionWrapper::set_session_id_header)
        .def_property("console_uid", &SessionWrapper::get_console_uid, &SessionWrapper::set_console_uid)
        .def_property("hashed_id_local", &SessionWrapper::get_hashed_id_local, &SessionWrapper::set_hashed_id_local)
        .def_property("hashed_id_console", &SessionWrapper::get_hashed_id_console, &SessionWrapper::set_hashed_id_console)
        .def_property("sock", &SessionWrapper::get_sock, &SessionWrapper::set_sock)
        .def_property("ctrl_sock", &SessionWrapper::get_ctrl_sock, &SessionWrapper::set_ctrl_sock)
        .def_property("data_sock", &SessionWrapper::get_data_sock, &SessionWrapper::set_data_sock)
        .def_property("account_id", &SessionWrapper::get_account_id, &SessionWrapper::set_account_id)
        .def_property("sid_local", &SessionWrapper::get_sid_local, &SessionWrapper::set_sid_local)
        .def_property("sid_console", &SessionWrapper::get_sid_console, &SessionWrapper::set_sid_console)
        .def_property("local_port_ctrl", &SessionWrapper::get_local_port_ctrl, &SessionWrapper::set_local_port_ctrl)
        .def_property("local_port_data", &SessionWrapper::get_local_port_data, &SessionWrapper::set_local_port_data)
        .def_property("stun_random_allocation", &SessionWrapper::get_stun_random_allocation, &SessionWrapper::set_stun_random_allocation)
        .def_property("ws_thread_should_stop", &SessionWrapper::get_ws_thread_should_stop, &SessionWrapper::set_ws_thread_should_stop)
        .def_property("ws_open", &SessionWrapper::get_ws_open, &SessionWrapper::set_ws_open)
        .def_property("main_should_stop", &SessionWrapper::get_main_should_stop, &SessionWrapper::set_main_should_stop)
        .def("__repr__", &SessionWrapper::to_string);
}
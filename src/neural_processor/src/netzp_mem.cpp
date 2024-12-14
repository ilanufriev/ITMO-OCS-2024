#include "netzp_mem.hpp"
#include "netzp_config.hpp"
#include "netzp_utils.hpp"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_module_name.h"
#include "sysc/kernel/sc_wait_cthread.h"
#include <iostream>
#include <iomanip>
#include <ostream>
#include <stdexcept>

namespace netzp {

constexpr char INDEX_OOB[] = "Memory index out of bounds";

MemReply::MemReply()
    : master_id(0)
    , op_type(MemOperationType::NONE)
    , status(MemOperationStatus::NONE)
    , data(0) {}

MemReply::MemReply(const MemReply& other)
    : master_id(other.master_id)
    , op_type(other.op_type)
    , status(other.status)
    , data(other.data) {}

bool MemReply::operator == (const MemReply& other) const {
    return master_id == other.master_id
        && op_type   == other.op_type
        && status    == other.status
        && data      == other.data
        && addr      == other.addr;
}

MemRequest::MemRequest()
    : master_id(0)
    , op_type(MemOperationType::NONE)
    , addr(0)
    , data_wr(0) {}

MemRequest::MemRequest(const MemRequest& other)
    : master_id(other.master_id)
    , op_type(other.op_type)
    , addr(other.addr)
    , data_wr(other.data_wr) {}

bool MemRequest::operator == (const MemRequest& other) const {
    return master_id == other.master_id
        && op_type   == other.op_type
        && data_wr   == other.data_wr
        && addr      == other.addr;
}

std::ostream& operator << (std::ostream& out, const MemReply& reply) {
    out << PRINTVAL(reply.data)      << std::endl
        << PRINTVAL(reply.addr)      << std::endl
        << PRINTVAL(reply.master_id) << std::endl
        << PRINTVAL(static_cast<int>(reply.op_type)) << std::endl
        << PRINTVAL(static_cast<int>(reply.status));
    return out;
}

std::ostream& operator << (std::ostream& out, const MemRequest& req) {
    out << PRINTVAL(req.master_id)                 << std::endl
        << PRINTVAL(req.data_wr)                   << std::endl
        << PRINTVAL(req.addr)                      << std::endl
        << PRINTVAL(static_cast<int>(req.op_type));
    return out;
}

std::vector<MemRequest> ReadMemorySpanRequests(mem_addr_t base_addr, size_t size,
                                               mem_master_id_t master_id) {
    std::vector<MemRequest> result;
    for (mem_addr_t addr = base_addr; addr < (base_addr + size); addr++) {
        auto& req     = result.emplace_back();

        req.addr      = addr;
        req.data_wr   = 0;
        req.master_id = master_id;
        req.op_type   = MemOperationType::READ;
    }

    return result;
}


std::vector<uchar> RepliesToByes(const std::vector<MemReply>& replies) {
    std::vector<uchar> bytes;
    for (const auto& reply : replies) {
        bytes.push_back(static_cast<uchar>(reply.data.to_uint()));
    }

    return bytes;
}

void Mem::AtClk() {
    if (rst.read()) {
        for (int i = 0; i < mem_.size(); i++) {
            mem_[i] = 0;
        }

        ack_out->write(0);
        data_rd->write(0);
    } else if (clk.read()) {
        if (r_en->read()) {
            data_rd->write(data_rd_next_);
        }
        ack_out->write(ack_next_);
    }
}

void Mem::MemAccess() {
    DEBUG_BEGIN_MODULE__;
    data_rd_next_ = 0;
    ack_next_     = 0;

    if (addr.read() >= mem_.size()) {
        throw std::invalid_argument(INDEX_OOB);
    }

    if (r_en->read()) {
        data_rd_next_ = mem_.at(addr->read());
        ack_next_ = true;
    } else if (w_en->read()) {
        mem_[addr->read()] = data_wr;
        ack_next_ = true;
    }
}

void Mem::AtAck() {
    DEBUG_BEGIN_MODULE__;
    if (ack_in->read() == true) {
        ack_next_ = false;
    }
}

void Mem::Dump(std::ostream& out) const {
    DEBUG_BEGIN_MODULE__;
    int row_w = 32;
    int hex_group_w = 2;

    for (int i = 0; i < mem_.size(); i += row_w) {
        for (int j = 0; (j < row_w) && (i + j < mem_.size()); j += 1) {
            if (j % hex_group_w == 0 && j != 0) {
                out << " ";
            }
            out << std::hex << std::noshowbase << std::setfill('0') << std::setw(2)
                << +static_cast<unsigned char>(mem_.at(i + j).to_uint());
        }
        out << std::endl;
    }
}

Mem::Mem(sc_core::sc_module_name const &modname, int memsize) : mem_(memsize) {
    SC_METHOD(AtClk);
    sensitive << clk.pos();

    SC_METHOD(MemAccess);
    sensitive << data_wr << addr << w_en << r_en;

    SC_METHOD(AtAck);
    sensitive << ack_in;
}

void MemController::AtClk() {
    if (rst->read()) {
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            access_granted[i]->write(false);
            replies_out[i]->write(MemReply());
        }

        data_wr->write(0);
        addr->write(0);
        w_en->write(0);
        r_en->write(0);
        ack_out->write(0);
    } else if (clk->read()) {

        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            access_granted[i]->write(access_granted_next_[i]);
        }

        replies_out[current_access_.read()]->write(reply_next_);

        data_wr->write(data_wr_next_);
        addr->write(addr_next_);
        w_en->write(w_en_next_);
        r_en->write(r_en_next_);
        ack_out->write(ack_out_next_);

        // Internal counter
        current_access_.write(current_access_next_);
    }
}

void MemController::AtRequest() {
    DEBUG_BEGIN_MODULE__;
    static constexpr int DEBUG_MSG_LEVEL = 3;

    addr_next_    = 0;
    w_en_next_    = 0;
    r_en_next_    = 0;
    data_wr_next_ = 0;

    if (access_granted[current_access_.read()]->read() == true) {
        request_ = requests_in[current_access_.read()]->read();

        DEBUG_OUT(DEBUG_MSG_LEVEL) << request_ << std::endl;
        switch (request_.op_type) {
            case netzp::MemOperationType::READ: {
                addr_next_    = request_.addr;
                w_en_next_    = 0;
                r_en_next_    = 1;
                data_wr_next_ = 0;
                break;
            }
            case netzp::MemOperationType::WRITE: {
                addr_next_    = request_.addr;
                w_en_next_    = 1;
                r_en_next_    = 0;
                data_wr_next_ = request_.data_wr;
                break;
            }
            case netzp::MemOperationType::NONE: {
                /* nothing */
            }
        }
    }
}

void MemController::AtAck() {
    DEBUG_BEGIN_MODULE__;
    static constexpr int DEBUG_MSG_LEVEL = 3;

    if (ack_in->read() == true) {
        if (request_.op_type == MemOperationType::READ) {
            reply_next_.data = data_rd->read();
        } else if (request_.op_type == MemOperationType::WRITE) {
            reply_next_.data = request_.data_wr;
        }

        reply_next_.addr = request_.addr;
        reply_next_.master_id = request_.master_id;
        reply_next_.op_type = request_.op_type;
        reply_next_.status = MemOperationStatus::OK;

        DEBUG_OUT(DEBUG_MSG_LEVEL) << reply_next_ << std::endl;

        ack_out_next_ = true;
    } else if (ack_in.read() == false) {
        ack_out_next_ = false;
    }
}

void MemController::AtCounter() {
    current_access_next_ = 0;

    if (access_request[current_access_.read()]->read() == true) {
        access_granted_next_[current_access_.read()] = true;
    } else {
        access_granted_next_[current_access_.read()] = false;
        if (current_access_.read() == (MAX_CONNECTIONS - 1)) {
            current_access_next_ = 0;
        } else {
            current_access_next_ = current_access_.read() + 1;
        }
    }
}

MemController::MemController(sc_core::sc_module_name const&) {
    SC_METHOD(AtClk);
    sensitive << clk.pos();

    SC_METHOD(AtRequest);
    for (const auto& request : requests_in) sensitive << request;
    sensitive << current_access_;

    SC_METHOD(AtAck);
    sensitive << ack_in;

    SC_METHOD(AtCounter);
    for (const auto& request : access_request) sensitive << request;
    sensitive << current_access_;
}

void MemIO::AtClk() {
    if (rst.read()) {
        request->write(MemRequest());
        requests_fifo_.clear();
        replies_fifo_.clear();
    }
}

void MemIO::MainProcess() {
    static constexpr int DEBUG_MSG_LEVEL = 3;

    // main loop
    while (true) {
        new_reply_ = false;

        sc_core::wait(); // wait for clock
        if (new_request_) {
            DEBUG_OUT(DEBUG_MSG_LEVEL) << "Request!" << std::endl;
            for (const auto& request : requests_from_host->read().data) {
                requests_fifo_.push_back(request);
            }

            new_request_ = false;
            access_request->write(true);
        }

        while (access_request.read() == true && access_granted.read() == true && !requests_fifo_.empty()) {
            sc_core::wait();
            DEBUG_OUT(DEBUG_MSG_LEVEL) << "Sending! " << requests_fifo_.size() << std::endl;
            request->write(requests_fifo_.front());

            if (new_reply_) {
                DEBUG_OUT(DEBUG_MSG_LEVEL) << "Reply" << std::endl;
                DEBUG_OUT(DEBUG_MSG_LEVEL) << reply->read() << std::endl;
                replies_fifo_.push_back(reply->read());
                requests_fifo_.pop_front();

                new_reply_ = false;
            }

            if (requests_fifo_.empty()) {
                DataVector<MemReply> ret;
                for (const auto& el : replies_fifo_) {
                    ret.data.push_back(el);
                }

                ret.data.shrink_to_fit();
                replies_to_host->write(ret);
                replies_fifo_.clear();
            }
        }

    }
}

void MemIO::AtRequestsFromHost() {
    new_request_ = true;
}

void MemIO::AtReply() {
    new_reply_ = true;
}

MemIO::MemIO(sc_core::sc_module_name const&) {
    SC_THREAD(MainProcess);
    sensitive << clk.pos();

    SC_METHOD(AtRequestsFromHost);
    sensitive << requests_from_host;

    SC_METHOD(AtReply);
    sensitive << reply;
}

} // namespace netzp
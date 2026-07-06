#pragma once
#include "integrations.hpp"

struct ModbusNamespace {
    // Helper to generate a generic Modbus Client object configured with RTU or TCP transport
    var _createClient(const std::string& transport, const std::string& target) {
        var s = var(var_object{});
        
        s["readCoils"] = var([transport, target](const var& slaveId, const var& address, const var& quantity) -> var {
            print("[Modbus " + transport + " Client] Read Coils from: " + target + ", Slave: " + slaveId.toString() + ", Addr: " + address.toString() + ", Qty: " + quantity.toString());
            var_array coils;
            for (int i = 0; i < quantity.toInt(); ++i) coils.push_back(var(i % 2 == 0));
            return coils;
        });

        s["readDiscreteInputs"] = var([transport, target](const var& slaveId, const var& address, const var& quantity) -> var {
            print("[Modbus " + transport + " Client] Read Discrete Inputs from: " + target + ", Slave: " + slaveId.toString() + ", Addr: " + address.toString() + ", Qty: " + quantity.toString());
            var_array inputs;
            for (int i = 0; i < quantity.toInt(); ++i) inputs.push_back(var(i % 3 != 0));
            return inputs;
        });

        s["readHoldingRegisters"] = var([transport, target](const var& slaveId, const var& address, const var& quantity) -> var {
            print("[Modbus " + transport + " Client] Read Holding Registers from: " + target + ", Slave: " + slaveId.toString() + ", Addr: " + address.toString() + ", Qty: " + quantity.toString());
            var_array regs;
            for (int i = 0; i < quantity.toInt(); ++i) regs.push_back(var(100 + i * 15));
            return regs;
        });

        s["readInputRegisters"] = var([transport, target](const var& slaveId, const var& address, const var& quantity) -> var {
            print("[Modbus " + transport + " Client] Read Input Registers from: " + target + ", Slave: " + slaveId.toString() + ", Addr: " + address.toString() + ", Qty: " + quantity.toString());
            var_array regs;
            for (int i = 0; i < quantity.toInt(); ++i) regs.push_back(var(500 + i * 8));
            return regs;
        });

        s["writeSingleCoil"] = var([transport, target](const var& slaveId, const var& address, const var& val) -> var {
            print("[Modbus " + transport + " Client] Write Single Coil to: " + target + ", Slave: " + slaveId.toString() + ", Addr: " + address.toString() + ", Value: " + val.toString());
            return true;
        });

        s["writeSingleRegister"] = var([transport, target](const var& slaveId, const var& address, const var& val) -> var {
            print("[Modbus " + transport + " Client] Write Single Register to: " + target + ", Slave: " + slaveId.toString() + ", Addr: " + address.toString() + ", Value: " + val.toString());
            return true;
        });

        s["writeMultipleCoils"] = var([transport, target](const var& slaveId, const var& address, const var& vals) -> var {
            print("[Modbus " + transport + " Client] Write Multiple Coils to: " + target + ", Slave: " + slaveId.toString() + ", Addr: " + address.toString() + ", Values: " + vals.toString());
            return true;
        });

        s["writeMultipleRegisters"] = var([transport, target](const var& slaveId, const var& address, const var& vals) -> var {
            print("[Modbus " + transport + " Client] Write Multiple Registers to: " + target + ", Slave: " + slaveId.toString() + ", Addr: " + address.toString() + ", Values: " + vals.toString());
            return true;
        });

        return s;
    }

    var _createServer(const std::string& transport, const std::string& target) {
        var s = var(var_object{});
        s.event_listeners = std::make_shared<std::map<std::string, std::vector<var>>>();
        
        s["holdingRegisters"] = var(var_object{});
        s["coils"] = var(var_object{});
        
        // Mock a write request from an external master
        setTimeout(var([s]() mutable {
            var reg_addr = var(40002);
            var reg_val = var(345);
            s["holdingRegisters"][reg_addr] = reg_val;
            s.emit(var("write"), var_object{{"type", var("register")}, {"address", reg_addr}, {"value", reg_val}});
        }), var(200));

        print("[Modbus " + transport + " Server] Listening on: " + target);
        return s;
    }

    var Client(const var& serialPort, const var& baudrate) {
        return _createClient("RTU", serialPort.toString() + " (" + baudrate.toString() + " bps)");
    }

    var RTUClient(const var& serialPort, const var& baudrate) {
        return _createClient("RTU", serialPort.toString() + " (" + baudrate.toString() + " bps)");
    }

    var TCPClient(const var& ip, const var& port) {
        return _createClient("TCP", ip.toString() + ":" + port.toString());
    }

    var RTUServer(const var& serialPort, const var& baudrate, const var& slaveId) {
        return _createServer("RTU", serialPort.toString() + " (" + baudrate.toString() + " bps), Slave: " + slaveId.toString());
    }

    var TCPServer(const var& port) {
        return _createServer("TCP", "Port " + port.toString());
    }
} Modbus;

struct HL7Namespace {
    var unframe(const var& mllpMsg) {
        std::string raw = mllpMsg.toString();
        if (raw.empty()) return var("");
        size_t start = 0;
        if (raw[0] == '\x0b') start = 1;
        size_t end = raw.length();
        if (end > start && raw[end - 1] == '\r') end--;
        if (end > start && raw[end - 1] == '\x1c') end--;
        return var(raw.substr(start, end - start));
    }

    var frame(const var& hl7Msg) {
        return var("\x0b" + hl7Msg.toString() + "\x1c\r");
    }

    var parse(const var& hl7Msg) {
        std::string raw = hl7Msg.toString();
        var msg_obj = var(var_object{});
        
        std::stringstream ss(raw);
        std::string segment_line;
        while (std::getline(ss, segment_line, '\r')) {
            if (segment_line.empty() || segment_line == "\n") continue;
            if (segment_line[0] == '\n') segment_line = segment_line.substr(1);
            
            std::stringstream seg_ss(segment_line);
            std::string field;
            std::vector<var> fields;
            
            while (std::getline(seg_ss, field, '|')) {
                std::stringstream field_ss(field);
                std::string comp;
                std::vector<var> components;
                while (std::getline(field_ss, comp, '^')) {
                    components.push_back(var(comp));
                }
                if (components.size() == 1) {
                    fields.push_back(components[0]);
                } else {
                    fields.push_back(var(var_array(components.begin(), components.end())));
                }
            }
            if (!fields.empty()) {
                std::string segment_name = fields[0].toString();
                msg_obj[segment_name] = var(var_array(fields.begin(), fields.end()));
            }
        }
        return msg_obj;
    }

    var ack(const var& parsedMsg, const var& ackCode = "AA") {
        var msh = parsedMsg["MSH"];
        std::string sending_app = msh.size() > 2 ? msh[2].toString() : "DOLPHIN";
        std::string sending_fac = msh.size() > 3 ? msh[3].toString() : "HOSPITAL";
        std::string control_id = msh.size() > 9 ? msh[9].toString() : "1";
        
        std::string ack = "MSH|^~\\&|DOLPHIN_HL7|HOSPITAL|" + sending_app + "|" + sending_fac + "|20260705120000||ACK|" + control_id + "|P|2.3\r"
                          "MSA|" + ackCode.toString() + "|" + control_id + "\r";
        return var(ack);
    }

    var Server() {
        var s = var(var_object{});
        s.event_listeners = std::make_shared<std::map<std::string, std::vector<var>>>();
        
        s["start"] = var([this, s](const var& port) mutable -> var {
            print("[HL7 Server] Started HL7 MLLP server on port: " + port.toString());
            var tcp_srv = TCP.Server();
            s["tcp_srv"] = tcp_srv;
            tcp_srv.on(var("connection"), var([this, s](const std::vector<var>& args) mutable -> var {
                var client = args[0];
                client.on(var("data"), var([this, s, client](const std::vector<var>& data_args) mutable -> var {
                    var raw_data = data_args[0];
                    var hl7_str = unframe(raw_data);
                    print("[HL7 Server] Incoming HL7 Message received:\n" + hl7_str.toString());
                    
                    var parsed = parse(hl7_str);
                    s.emit(var("message"), var_object{{"message", parsed}, {"raw", hl7_str}});
                    
                    var ack_msg = ack(parsed);
                    client.write(frame(ack_msg));
                    return var();
                }));
                return var();
            }));
            tcp_srv.listen(port);
            return true;
        });
        
        return s;
    }
} HL7;

#include "odrive.h"

/*
 * Constructor
 * Initailize USB library
 */

dhr::odrive::odrive(){
		if(libusb_init(&libusb_context_) != LIBUSB_SUCCESS){
				std::cout << "Error occurred while initializing USB" << std::endl;
		}
}

/*
 * Destructor
 *
 */

dhr::odrive::~odrive(){
		if(libusb_context_ != NULL){
				libusb_exit(libusb_context_);
				libusb_context_ = NULL;
		}
}

/**
 *
 * Append short data to data buffer
 * @param buf data buffer
 * @param value data to append
 *
 */
void dhr::odrive::appendShortToCommBuffer(commBuffer& buf, const short value)
{
    buf.push_back((value >> 0) & 0xFF);
    buf.push_back((value >> 8) & 0xFF);
}

/**
 *
 * Append int data to data buffer
 * @param buf data buffer
 * @param value data to append
 *
 */
void dhr::odrive::appendIntToCommBuffer(commBuffer& buf, const int value)
{
    buf.push_back((value >> 0) & 0xFF);
    buf.push_back((value >> 8) & 0xFF);
    buf.push_back((value >> 16) & 0xFF);
    buf.push_back((value >> 24) & 0xFF);
}


/**
 *
 *  Decode odrive packet
 *  @param buf data buffer
 *  @param seq_no packet sequence number
 *  @param received_packet received buffer
 *  @return data buffer
 *
 */
commBuffer dhr::odrive::decodeODrivePacket(commBuffer& buf,
    	short& seq_no, commBuffer& received_packet)
{
    commBuffer payload;

	memcpy(&seq_no, &buf[0], sizeof(short));
    seq_no &= 0x7fff;
    for (commBuffer::size_type i = 2; i < buf.size(); ++i) {
        payload.push_back(buf[i]);
    }
    return payload;
}

/**
 *
 * Read data buffer from Odrive harware
 * @param seq_no next sequence number
 * @param endpoint_id USB endpoint ID
 * @param response_size maximum data length to be read
 * @param read append request address
 * @param address desctination address
 * @param input data buffer to send
 * @return data buffer read
 *
 */
commBuffer dhr::odrive::createODrivePacket(short seq_no, int endpoint_id,
                short response_size, bool read, int address, const commBuffer& input)
{
    commBuffer packet;
    short crc = 0;

    if ((endpoint_id & 0x7fff) == 0) {
        crc = ODRIVE_PROTOCOL_VERSION;
    }
    else {
        crc = ODRIVE_DEFAULT_CRC_VALUE;
    }

    appendShortToCommBuffer(packet, seq_no);
    appendShortToCommBuffer(packet, endpoint_id);
    appendShortToCommBuffer(packet, response_size);
    if (read) {
        appendIntToCommBuffer(packet, address);
    }

    for (uint8_t b : input) {
        packet.push_back(b);
    }

    appendShortToCommBuffer(packet, crc);

    return packet;
}

/**
 *
 *  Read value from ODrive
 *  @param id odrive ID
 *  @param value Data read
 *  @return ODRIVE_OK on success
 *
 */
template<typename T>
int dhr::odrive::getData(int id, T& value)
{
    commBuffer tx;
    commBuffer rx;
    int rx_size;

    int result = endpointRequest(id, rx,
                    rx_size, tx, 1 /* ACK */, sizeof(value));
    if (result != LIBUSB_SUCCESS) {
        return result;
    }

    memcpy(&value, &rx[0], sizeof(value));

    return LIBUSB_SUCCESS;
}

/**
 *
 *  Request function to ODrive
 *  @param id odrive ID
 *  @return ODRIVE_OK on success
 *
 */
int dhr::odrive::execFunc(int endpoint_id)
{
    commBuffer tx;
    commBuffer rx;
    int rx_length;
    int status;

    status = endpointRequest(endpoint_id, rx, rx_length, tx, 1, 0);
    if (status != LIBUSB_SUCCESS) {
			std::cout << "* execFunc: Error in endpoint request" << std::to_string(endpoint_id) << std::endl;
    }
    return status;
}

/**
 *
 *  Write value to Odrive
 *  @param id odrive ID
 *  @param value Data to be written
 *  @return ODRIVE_OK on success
 *
 */
template<typename TT>
int dhr::odrive::setData(int endpoint_id, const TT& value)
{
    commBuffer tx;
    commBuffer rx;
    int rx_length;

    for(int i = 0; i < sizeof(value); i++){
       tx.push_back(((unsigned char*)&value)[i]);
    }

    return endpointRequest(endpoint_id, rx, rx_length, tx, 1, 0);
}

/**
 *
 * Request endpoint
 * @param handle USB device handler
 * @param endpoint_id odrive ID
 * @param received_payload receive buffer
 * @param received_length receive length
 * @param payload data read
 * @param ack request acknowledge
 * @param length data length
 * @param read send read address
 * @param address read address
 * @return LIBUSB_SUCCESS on success
 *
 */
int dhr::odrive::endpointRequest(int endpoint_id, commBuffer& received_payload,
    	int& received_length, commBuffer payload,
    	bool ack, int length, bool read, int address)
{
    commBuffer send_buffer;
    commBuffer receive_buffer;
    unsigned char receive_bytes[ODRIVE_MAX_RESULT_LENGTH] = { 0 };
    int sent_bytes = 0;
    int received_bytes = 0;
    short received_seq_no = 0;

    ep_lock.lock();

    // Prepare sequence number
    if (ack) {
        endpoint_id |= 0x8000;
    }
    outbound_seq_no_ = (outbound_seq_no_ + 1) & 0x7fff;
    outbound_seq_no_ |= LIBUSB_ENDPOINT_IN;
    short seq_no = outbound_seq_no_;

    // Create request packet
    commBuffer packet = createODrivePacket(seq_no, endpoint_id, length, read, address, payload);

    // Transfer paket to target
    int result = libusb_bulk_transfer(odrive_handle_, ODRIVE_OUT_EP,
    	    packet.data(), packet.size(), &sent_bytes, ODRIVE_TIMEOUT);
    if (result != LIBUSB_SUCCESS) {
			std:: cout << "* Error in transfering data to USB!" << std::endl;
        ep_lock.unlock();
        return result;
    } else if (packet.size() != sent_bytes) {
			std::cout << "* Error in transfering data to USB, not all data transferred!" << std::endl;

    }

    // Get responce
    if (ack) {
        result = libusb_bulk_transfer(odrive_handle_, ODRIVE_IN_EP,
    		receive_bytes, ODRIVE_MAX_BYTES_TO_RECEIVE,
    		&received_bytes, ODRIVE_TIMEOUT);
        if (result != LIBUSB_SUCCESS) {
		    std::cout << "* Error in reading data from USB!" <<  std::endl;
            ep_lock.unlock();
            return result;
        }

        // Push recevived data to buffer
        for (int i = 0; i < received_bytes; i++) {
            receive_buffer.push_back(receive_bytes[i]);
        }

        received_payload = decodeODrivePacket(receive_buffer, received_seq_no, receive_buffer);
        if (received_seq_no != seq_no) {
				std::cout << "* Error Received data out of order" << std::endl;
        }
        received_length = received_payload.size();

    }

    ep_lock.unlock();

    return LIBUSB_SUCCESS;
}



/*
 * Endpoint initialization function
 * @param Odrive Serial number
 * @return boolean represetaition of the initialization success
 */

int dhr::odrive::init(uint64_t serialNumber)
{
    libusb_device ** usb_device_list;
    int ret = 1;
    
    ssize_t device_count = libusb_get_device_list(libusb_context_, &usb_device_list);
    std::cout << device_count << std::endl;
    if (device_count <= 0) {
        return device_count;
    }

    for (size_t i = 0; i < device_count; ++i) {
        libusb_device *device = usb_device_list[i];
        libusb_device_descriptor desc = {0};

        int result = libusb_get_device_descriptor(device, &desc);
        if (result != LIBUSB_SUCCESS) {
				std:: cout << "* Error getting device descriptor" << std::endl;
            continue;
        }
        /* Check USB devicei ID */
        if (desc.idVendor == ODRIVE_USB_VENDORID && desc.idProduct == ODRIVE_USB_PRODUCTID) {

            libusb_device_handle *device_handle;
            if (libusb_open(device, &device_handle) != LIBUSB_SUCCESS) {
                std ::cout << "* Error opeening USB device" << std::endl;
                continue;
             }

            struct libusb_config_descriptor *config;
            result = libusb_get_config_descriptor(device, 0, &config);
            int ifNumber = 2; //config->bNumInterfaces;

            if ((libusb_kernel_driver_active(device_handle, ifNumber) != LIBUSB_SUCCESS) &&
                    (libusb_detach_kernel_driver(device_handle, ifNumber) != LIBUSB_SUCCESS)) {
					std:: cout << "* Driver error" << std::endl;
                libusb_close(device_handle);
                continue;
            }

            if ((result = libusb_claim_interface(device_handle, ifNumber)) !=  LIBUSB_SUCCESS) {
					std::cout << "* Error claiming device" << std::endl;
                libusb_close(device_handle);
                continue;
            } else {
                bool attached_to_handle = false;
                unsigned char buf[128];

 		result = libusb_get_string_descriptor_ascii(device_handle, desc.iSerialNumber, buf, 127);
                if (result <= 0) {
						std::cout << "* Error getting data" << std::endl;
                    result = libusb_release_interface(device_handle, ifNumber);
                    libusb_close(device_handle);
                    continue;
                } else {
                    std::stringstream stream;
                    stream << std::uppercase << std::hex << serialNumber;
                    std::string sn(stream.str());

                    if (sn.compare(0, strlen((const char*)buf), (const char*)buf) == 0) {
							std:: cout << "Device " << serialNumber << " found" << std::endl;
                        odrive_handle_ = device_handle;
                        attached_to_handle = true;
                        ret = ODRIVE_OK;
                        break;
                    }
                }
                if (!attached_to_handle) {
                    result = libusb_release_interface(device_handle, ifNumber);
                    libusb_close(device_handle);
                }
            }
        }
    }

    libusb_free_device_list(usb_device_list, 1);

    return ret;
}

/**
 *
 * Odrive endpoint close
 * close ODrive dvice
 *
 */
void dhr::odrive::close(void)
{
    if (odrive_handle_ != NULL) {
        libusb_release_interface(odrive_handle_, 2);
        libusb_close(odrive_handle_);
        odrive_handle_ = NULL;
    }
}

/**
 *
 *  Scan for object name in target JSON
 *  @param odrive_json target json
 *  @param name object name to be found
 *  @param odo odrive object pointer including object parameters
 *  @return ODRIVE_OK on success
 *
 */

int dhr::getObjectByName(Json::Value odrive_json, std::string name, dhr::odrive_object *odo)
{
    int ret = -1;
    int i, pos;
    std::string token;
    Json::Value js;
    Json::Value js2 = odrive_json;

    while ((pos = name.find(".")) != std::string::npos) {
    js = js2;
        token = name.substr(0, pos);
        for (i = 0 ; i < js.size() ; i++) {
            if (!token.compare(js[i]["name"].asString())) {
    	if (!std::string("object").compare(js[i]["type"].asString())) {
                js2 = js[i]["members"];
    	}
    	else {
                    js2 = js[i];
    	}
    	break;
            }
        }
    name.erase(0, pos + 1);
    }

    for (i = 0 ; i < js2.size() ; i++) {
        if (!name.compare(js2[i]["name"].asString())) {
            odo->name = js2[i]["name"].asString();
            odo->id = js2[i]["id"].asInt();
            odo->type = js2[i]["type"].asString();
            odo->access = js2[i]["access"].asString();
        ret = 0;
        break;
        }
    }

    if (ret) {
        std::cout << name.c_str() << " not found!\n";
    }
    return ret;
}

/**
 *
 *  Read JSON file from target
 *  @param endpoint odrive enumarated endpoint
 *  @param odrive_json pointer to target json object
 *
 */
int dhr::getJson(dhr::odrive *endpoint, Json::Value *odrive_json)
{

    commBuffer rx;
    commBuffer tx;
    int len;
    int address = 0;
    std::string json;

    do {
        endpoint->endpointRequest(0, rx, len, tx, true, 64, true, address);
        address = address + len;
        json.append((const char *)&rx[0], (size_t)len);
    } while (len > 0);

    Json::Reader reader;
    bool res = reader.parse(json, *odrive_json);
    if (!res) {
        std::cout <<  "* Error parsing json!" << std::endl;
        return 1;
    }
    return 0;
}

/**
 *
 *  Read single value from target
 *  @param endpoint odrive enumarated endpoint
 *  @param odrive_json target json
 *  @param value return value
 *  @param command odrive comand
 *  @return ODRIVE_OK on success
 *
 */
template<typename TT>
int dhr::readOdriveData(dhr::odrive *endpoint, Json::Value odrive_json,
    	std::string command, TT &value)
{
    int ret;
    odrive_object odo;

    ret = getObjectByName(odrive_json, command, &odo);

	if(ret != ODRIVE_OK){

		return ODRIVE_FAILED;
	}

    ret = endpoint->getData(odo.id, value);

    return ret;
}

/**
 *
 *  Write single value to target
 *  @param endpoint odrive enumarated endpoint
 *  @param odrive_json target json
 *  @param value value to be written
 *  @return ODRIVE_OK on success
 *
 */
template<typename T>
int dhr::writeOdriveData(dhr::odrive *endpoint, Json::Value odrive_json,
                std::string command, T &value)
{
    int ret;
    odrive_object odo;

    ret = getObjectByName(odrive_json, command, &odo);

		if(ret != ODRIVE_OK){
			return ODRIVE_FAILED;
		}

    ret = endpoint->setData(odo.id, value);

    return ret;
}

/**
 *
 *  Exec target function
 *  @param endpoint odrive enumarated endpoint
 *  @param odrive_json target json
 *  @param object name
 *  @return ODRIVE_OK on success
 *
 */
int dhr::execOdriveFunc(dhr::odrive *endpoint, Json::Value odrive_json,
                std::string object)
{
    int ret;
    odrive_object odo;

    ret = getObjectByName(odrive_json, object, &odo);
    if (ret) {
        std:: cout << "* Error getting ID" << std::endl;
        return ret;
    }

    if (odo.type.compare("function")) {
        std:: cout << "* Error invalid type" << std::endl;
        return ret;
    }

    ret = endpoint->execFunc(odo.id);
    if (ret != LIBUSB_SUCCESS) {
        std::cout << "* Error executing "<< object.c_str() << " function" << std::endl;
    }
    return ret;
}


template int dhr::odrive::getData(int, bool&);
template int dhr::odrive::getData(int, short&);
template int dhr::odrive::getData(int, int&);
template int dhr::odrive::getData(int, float&);
template int dhr::odrive::getData(int, uint8_t&);
template int dhr::odrive::getData(int, uint16_t&);
template int dhr::odrive::getData(int, uint32_t&);
template int dhr::odrive::getData(int, uint64_t&);

template int dhr::odrive::setData(int, const bool&);
template int dhr::odrive::setData(int, const short&);
template int dhr::odrive::setData(int, const int&);
template int dhr::odrive::setData(int, const float&);
template int dhr::odrive::setData(int, const uint8_t&);
template int dhr::odrive::setData(int, const uint16_t&);
template int dhr::odrive::setData(int, const uint32_t&);
template int dhr::odrive::setData(int, const uint64_t&);


template int dhr::writeOdriveData(dhr::odrive *, Json::Value, std::string, uint8_t &);
template int dhr::writeOdriveData(dhr::odrive *, Json::Value, std::string, uint16_t &);
template int dhr::writeOdriveData(dhr::odrive *, Json::Value, std::string, uint32_t &);
template int dhr::writeOdriveData(dhr::odrive *, Json::Value, std::string, uint64_t &);
template int dhr::writeOdriveData(dhr::odrive *, Json::Value, std::string, int &);
template int dhr::writeOdriveData(dhr::odrive *, Json::Value, std::string, short &);
template int dhr::writeOdriveData(dhr::odrive *, Json::Value, std::string, float &);
template int dhr::writeOdriveData(dhr::odrive *, Json::Value, std::string, bool &);

template int dhr::readOdriveData(dhr::odrive*, Json::Value, std::string, uint8_t &);
template int dhr::readOdriveData(dhr::odrive*, Json::Value, std::string, uint16_t &);
template int dhr::readOdriveData(dhr::odrive*, Json::Value, std::string, uint32_t &);
template int dhr::readOdriveData(dhr::odrive*, Json::Value, std::string, uint64_t &);
template int dhr::readOdriveData(dhr::odrive*, Json::Value, std::string, int &);
template int dhr::readOdriveData(dhr::odrive*, Json::Value, std::string, short &);
template int dhr::readOdriveData(dhr::odrive*, Json::Value, std::string, float &);
template int dhr::readOdriveData(dhr::odrive*, Json::Value, std::string, bool &); 

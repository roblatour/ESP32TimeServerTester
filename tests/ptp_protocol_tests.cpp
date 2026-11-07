#define PTP_TESTER_HOST_TEST
#include "../main/ptp_auth.cpp"
#include "../main/ptp_tester.cpp"
#include "../main/ntp_load_schedule.h"

#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace ptp_tester;
    using Bytes = std::vector<uint8_t>;

    constexpr uint8_t Domain = 7;
    constexpr std::array<uint8_t, 10> Master = {0x10, 0x20, 0x30, 0xff, 0xfe, 0x40, 0x50, 0x60, 0, 1};
    constexpr std::array<uint8_t, 10> Requester = {0x02, 0x11, 0x22, 0xff, 0xfe, 0x33, 0x44, 0x55, 0, 2};
    size_t assertions = 0;

    void check(bool condition, const std::string &description)
    {
        ++assertions;
        if (!condition)
            throw std::runtime_error(description);
    }

    void put(Bytes &bytes, size_t offset, uint64_t value, size_t width)
    {
        for (size_t index = 0; index < width; ++index)
            bytes.at(offset + width - index - 1) = static_cast<uint8_t>(value >> (index * 8));
    }

    void stamp(Bytes &bytes, size_t offset, uint64_t seconds, uint32_t nanoseconds)
    {
        put(bytes, offset, seconds, 6);
        put(bytes, offset + 6, nanoseconds, 4);
    }

    Bytes golden(uint8_t type, uint8_t minor = 1, uint16_t sequence = 0x1234)
    {
        const size_t length = type == 11 ? 64 : type == 9 ? 54 : 44;
        Bytes bytes(length, 0);
        bytes[0] = type;
        bytes[1] = static_cast<uint8_t>((minor << 4) | 2);
        put(bytes, 2, length, 2);
        bytes[4] = Domain;
        put(bytes, 6, type == 0 ? TwoStepFlag : type == 11 ? 0x003c : 0, 2);
        std::copy(Master.begin(), Master.end(), bytes.begin() + 20);
        put(bytes, 30, sequence, 2);
        bytes[32] = type == 0 ? 0 : type == 1 ? 1 : type == 8 ? 2 : type == 9 ? 3 : 5;
        bytes[33] = 0x7f;
        stamp(bytes, 34, 0x010203040506ULL, 123456789);
        if (type == 9)
            std::copy(Requester.begin(), Requester.end(), bytes.begin() + 44);
        if (type == 11)
        {
            put(bytes, 44, 37, 2);
            bytes[47] = 128;
            bytes[48] = 6;
            bytes[49] = 0x21;
            put(bytes, 50, 0x1234, 2);
            bytes[52] = 129;
            std::copy(Master.begin(), Master.begin() + 8, bytes.begin() + 53);
            put(bytes, 61, 2, 2);
            bytes[63] = 0x20;
        }
        return bytes;
    }

    Message parse_valid(const Bytes &bytes)
    {
        Message message{};
        check(parse_message(bytes.data(), bytes.size(), Domain, message) == ParseResult::Valid, "golden packet rejected");
        return message;
    }

    void expect(const Bytes &bytes, ParseResult expected, const std::string &description)
    {
        Message message{};
        check(parse_message(bytes.data(), bytes.size(), Domain, message) == expected, description);
    }

    void append_tlv(Bytes &bytes, uint16_t type, const Bytes &payload)
    {
        const size_t offset = bytes.size();
        bytes.resize(offset + 4 + payload.size());
        put(bytes, offset, type, 2);
        put(bytes, offset + 2, payload.size(), 2);
        std::copy(payload.begin(), payload.end(), bytes.begin() + static_cast<ptrdiff_t>(offset + 4));
        put(bytes, 2, bytes.size(), 2);
    }

    void golden_messages()
    {
        for (uint8_t minor : std::initializer_list<uint8_t>{0, 1})
        {
            for (uint8_t type : std::initializer_list<uint8_t>{0, 1, 8, 9, 11})
            {
                const Message message = parse_valid(golden(type, minor));
                check(message.type == type && message.minor_version == minor, "message type/version decoded");
                check(message.sequence == 0x1234 && message.correction == 0, "sequence/correction decoded");
                check(std::equal(Master.begin(), Master.end(), message.source), "source identity decoded");
                check(message.timestamp.seconds == 0x010203040506ULL && message.timestamp.nanoseconds == 123456789,
                      "48-bit seconds and nanoseconds decoded");
                if (type == 9)
                    check(std::equal(Requester.begin(), Requester.end(), message.requester), "requester identity decoded");
                if (type == 11)
                {
                    check(message.flags == 0x003c && message.utc_offset == 37, "announce time properties decoded");
                    check(message.priority1 == 128 && message.priority2 == 129 && message.clock_class == 6 &&
                          message.accuracy == 0x21 && message.variance == 0x1234, "announce quality decoded");
                    check(message.steps == 2 && message.time_source == 0x20 &&
                          std::equal(Master.begin(), Master.begin() + 8, message.grandmaster), "announce topology decoded");
                }
            }
        }
        auto bytes = golden(11);
        for (int16_t offset : {int16_t{0}, int16_t{-37}, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max()})
        {
            put(bytes, 44, static_cast<uint16_t>(offset), 2);
            check(parse_valid(bytes).utc_offset == offset, "signed UTC offset decoded");
        }
    }

    void header_and_truncation()
    {
        Message message{};
        message.sequence = 123;
        check(parse_message(nullptr, 100, Domain, message) == ParseResult::Malformed && message.sequence == 0,
              "null input rejected and output reset");
        for (uint8_t type : std::initializer_list<uint8_t>{0, 1, 8, 9, 11})
        {
            const Bytes bytes = golden(type);
            for (size_t length = 0; length < bytes.size(); ++length)
            {
                Bytes prefix(bytes.begin(), bytes.begin() + static_cast<ptrdiff_t>(length));
                expect(prefix, ParseResult::Malformed, "every golden truncation rejected");
            }
            auto short_body = bytes;
            short_body.resize(bytes.size() - 2);
            put(short_body, 2, short_body.size(), 2);
            expect(short_body, ParseResult::Malformed, "declared short body rejected");
            auto padded = bytes;
            padded.resize(bytes.size() + 20, 0xff);
            expect(padded, ParseResult::Valid, "Ethernet padding outside message ignored");
            auto bad_control = golden(type, 0);
            bad_control[32] ^= 0x7f;
            expect(bad_control, ParseResult::Malformed, "v2.0 control field rejected");
        }
        for (uint16_t length : {uint16_t{0}, uint16_t{32}, uint16_t{33}, uint16_t{43}, uint16_t{45}, uint16_t{258}, uint16_t{65535}})
        {
            auto bytes = golden(0);
            bytes.resize(300);
            put(bytes, 2, length, 2);
            expect(bytes, ParseResult::Malformed, "invalid declared length rejected");
        }
        auto bytes = golden(0);
        bytes[4] = Domain + 1;
        expect(bytes, ParseResult::Ignore, "other domain ignored");
        for (uint8_t version : std::initializer_list<uint8_t>{0, 1, 3, 0x22, 0xf2})
        {
            bytes = golden(0);
            bytes[1] = version;
            expect(bytes, ParseResult::Unsupported, "unsupported version rejected");
        }
        for (uint8_t transport = 1; transport < 16; ++transport)
        {
            bytes = golden(0);
            bytes[0] |= static_cast<uint8_t>(transport << 4);
            expect(bytes, ParseResult::Unsupported, "transportSpecific rejected");
        }
        bytes = golden(0);
        bytes[5] = 1;
        expect(bytes, ParseResult::Unsupported, "nonzero reserved domain byte rejected");
        for (size_t index = 16; index < 20; ++index)
        {
            bytes = golden(0);
            bytes[index] = 1;
            expect(bytes, ParseResult::Unsupported, "reserved header byte rejected");
        }
        for (uint16_t flags : {uint16_t{0x0040}, uint16_t{0x0100}, uint16_t{0x0400}, uint16_t{0x8000}})
        {
            bytes = golden(0);
            put(bytes, 6, flags, 2);
            expect(bytes, ParseResult::Unsupported, "unsupported flags rejected");
        }
        for (uint8_t type : std::initializer_list<uint8_t>{2, 3, 4, 5, 6, 7, 10, 12, 13, 14, 15})
        {
            bytes = golden(0);
            bytes[0] = type;
            expect(bytes, ParseResult::Ignore, "unhandled message type ignored");
        }
    }

    void versioned_control_fields()
    {
        constexpr char Key[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
        Authenticator auth;
        check(auth.initialize(0x1234, Key, 7), "version compatibility synthetic key initialized");
        for (const uint8_t type : {uint8_t{11}, uint8_t{8}, uint8_t{9}, uint8_t{1}, uint8_t{0}})
        {
            const uint8_t legacy_control = type == 0 ? 0 : type == 1 ? 1 : type == 8 ? 2 : type == 9 ? 3 : 5;
            for (const uint8_t minor : {uint8_t{0}, uint8_t{1}})
            {
                for (const uint8_t control : {uint8_t{0}, legacy_control, uint8_t{0x7f}, uint8_t{0xff}})
                {
                    auto bytes = golden(type, minor);
                    bytes[32] = control;
                    expect(bytes, minor == 1 || control == legacy_control ? ParseResult::Valid : ParseResult::Malformed,
                           "v2.0 validates legacy control; v2.1 ignores deprecated control for every message type");
                    if (minor == 0)
                        continue;
                    size_t length = bytes.size();
                    bytes.resize(MaxMessageSize);
                    check(auth.append(bytes.data(), bytes.size(), length), "v2.1 control fixture signed");
                    Message message{};
                    check(parse_message(bytes.data(), length, Domain, message, &auth) == ParseResult::Valid &&
                              message.authenticated, "signed zero, legacy and arbitrary v2.1 controls verified");
                    bytes[32] ^= 1;
                    check(parse_message(bytes.data(), length, Domain, message, &auth) == ParseResult::AuthenticationInvalid,
                          "ignored v2.1 control field remains covered by authentication");
                }
            }
        }
    }

    void v21_header_promotion()
    {
        for (const uint8_t type : {uint8_t{0}, uint8_t{1}, uint8_t{8}, uint8_t{9}, uint8_t{11}})
        {
            auto bytes = golden(type, 0);
            auto expected = bytes;
            expected[1] = 0x12;
            expected[32] = 0;
            promote_to_v21(bytes.data());
            check(bytes == expected, "normal signed traffic upgrades only version and deprecated control bytes");
            bytes[32] = 5;
            expected[32] = 5;
            promote_to_v21(bytes.data());
            check(bytes == expected, "an existing v2.1 control-field probe is preserved before signing");
        }
    }

    void identities_timestamps_and_announce()
    {
        for (uint8_t type : std::initializer_list<uint8_t>{0, 8, 9, 11})
        {
            for (uint32_t nanos : {0U, 999999999U, 1000000000U, 0xffffffffU})
            {
                auto bytes = golden(type);
                stamp(bytes, 34, 0xffffffffffffULL, nanos);
                expect(bytes, nanos < 1000000000U ? ParseResult::Valid : ParseResult::Malformed, "nanosecond boundary");
            }
            for (uint8_t fill : std::initializer_list<uint8_t>{0, 255})
            {
                auto bytes = golden(type);
                std::fill(bytes.begin() + 20, bytes.begin() + 28, fill);
                expect(bytes, ParseResult::Malformed, "invalid source clock identity rejected");
            }
            for (uint16_t port : {uint16_t{0}, uint16_t{65535}})
            {
                auto bytes = golden(type);
                put(bytes, 28, port, 2);
                expect(bytes, ParseResult::Malformed, "invalid source port rejected");
            }
        }
        for (uint8_t fill : std::initializer_list<uint8_t>{0, 255})
        {
            auto bytes = golden(9);
            std::fill(bytes.begin() + 44, bytes.begin() + 52, fill);
            expect(bytes, ParseResult::Malformed, "invalid requester clock identity rejected");
            bytes = golden(11);
            std::fill(bytes.begin() + 53, bytes.begin() + 61, fill);
            expect(bytes, ParseResult::Malformed, "invalid grandmaster identity rejected");
        }
        for (uint16_t port : {uint16_t{0}, uint16_t{65535}})
        {
            auto bytes = golden(9);
            put(bytes, 52, port, 2);
            expect(bytes, ParseResult::Malformed, "invalid requester port rejected");
        }
        for (const auto &change : std::array<std::array<uint8_t, 2>, 7>{{{46, 1}, {48, 0}, {48, 255}, {49, 0x1f}, {49, 0x32}, {49, 0xff}, {63, 0x70}}})
        {
            auto bytes = golden(11);
            bytes[change[0]] = change[1];
            expect(bytes, change[0] == 63 ? ParseResult::Unsupported : ParseResult::Malformed, "announce invalid field rejected");
        }
        for (uint16_t steps : {uint16_t{254}, uint16_t{255}, uint16_t{65535}})
        {
            auto bytes = golden(11);
            put(bytes, 61, steps, 2);
            expect(bytes, steps < 255 ? ParseResult::Valid : ParseResult::Malformed, "stepsRemoved boundary");
        }
        for (uint8_t accuracy : std::initializer_list<uint8_t>{0x20, 0x31, 0xfe})
        {
            auto bytes = golden(11);
            bytes[49] = accuracy;
            expect(bytes, ParseResult::Valid, "supported accuracy accepted");
        }
        for (uint8_t source : std::initializer_list<uint8_t>{0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x90, 0xa0})
        {
            auto bytes = golden(11);
            bytes[63] = source;
            expect(bytes, ParseResult::Valid, "supported time source accepted");
        }
        for (uint16_t flags : {uint16_t{0}, uint16_t{0x8}, uint16_t{0x9}, uint16_t{0xa}, uint16_t{0xb}, uint16_t{1}, uint16_t{2}, uint16_t{4}})
        {
            auto bytes = golden(11);
            put(bytes, 6, flags, 2);
            const bool valid = flags == 0 || flags == 8 || flags == 9 || flags == 10;
            expect(bytes, valid ? ParseResult::Valid : ParseResult::Malformed, "leap/timescale consistency");
        }
        const uint8_t zero_mac[6] = {};
        const uint8_t multicast[6] = {1, 2, 3, 4, 5, 6};
        const uint8_t unicast[6] = {2, 2, 3, 4, 5, 6};
        check(!valid_target(zero_mac) && !valid_target(multicast) && valid_target(unicast), "target MAC validation");
    }

    void tlv_boundaries_and_authentication()
    {
        const Bytes identity(Master.begin(), Master.begin() + 8);
        auto bytes = golden(11);
        append_tlv(bytes, 8, identity);
        expect(bytes, ParseResult::Valid, "path trace accepted");
        append_tlv(bytes, 8, identity);
        expect(bytes, ParseResult::Valid, "multiple bounded TLVs accepted");
        bytes = golden(11);
        Bytes maximum_path(184);
        for (size_t index = 0; index < maximum_path.size(); ++index)
            maximum_path[index] = identity[index % identity.size()];
        append_tlv(bytes, 8, maximum_path);
        append_tlv(bytes, 0x8009, {});
        check(bytes.size() == MaxMessageSize, "maximum size fixture");
        expect(bytes, ParseResult::AuthenticationUnsupported, "authentication at maximum boundary rejected");
        bytes = golden(11);
        append_tlv(bytes, 8, maximum_path);
        expect(bytes, ParseResult::Valid, "large path trace accepted");
        bytes.resize(MaxMessageSize + 2);
        put(bytes, 2, bytes.size(), 2);
        expect(bytes, ParseResult::Malformed, "over maximum size rejected");
        for (uint8_t type : std::initializer_list<uint8_t>{0, 1, 8, 9, 11})
        {
            for (size_t payload_size : {size_t{0}, size_t{2}, size_t{16}})
            {
                bytes = golden(type);
                append_tlv(bytes, 0x8009, Bytes(payload_size, 0x5a));
                expect(bytes, ParseResult::AuthenticationUnsupported, "authentication never accepted");
            }
            bytes = golden(type);
            append_tlv(bytes, 0x8009, Bytes(2, 0));
            put(bytes, bytes.size() - 4, 4, 2);
            expect(bytes, ParseResult::Malformed, "overrunning authentication TLV rejected");
            bytes = golden(type);
            append_tlv(bytes, 8, identity);
            expect(bytes, type == 11 ? ParseResult::Valid : ParseResult::Unsupported, "path trace only on announce");
            bytes = golden(type);
            append_tlv(bytes, 0x1234, {});
            expect(bytes, ParseResult::Unsupported, "unknown TLV rejected");
        }
        for (size_t payload_size : {size_t{0}, size_t{2}, size_t{6}, size_t{10}})
        {
            bytes = golden(11);
            append_tlv(bytes, 8, Bytes(payload_size, 1));
            expect(bytes, ParseResult::Malformed, "path trace payload boundary rejected");
        }
        for (uint8_t fill : std::initializer_list<uint8_t>{0, 255})
        {
            bytes = golden(11);
            append_tlv(bytes, 8, identity);
            append_tlv(bytes, 8, Bytes(8, fill));
            expect(bytes, ParseResult::Malformed, "every path identity validated");
        }
        bytes = golden(11);
        bytes.resize(66);
        put(bytes, 2, bytes.size(), 2);
        expect(bytes, ParseResult::Malformed, "truncated TLV header rejected");
        bytes = golden(11);
        append_tlv(bytes, 8, identity);
        put(bytes, 66, 9, 2);
        expect(bytes, ParseResult::Malformed, "odd TLV length rejected");
        put(bytes, 66, 10, 2);
        expect(bytes, ParseResult::Malformed, "overrunning TLV rejected");
        bytes = golden(11);
        append_tlv(bytes, 8, identity);
        append_tlv(bytes, 0x8009, Bytes(2, 0));
        expect(bytes, ParseResult::AuthenticationUnsupported, "authentication after path trace rejected");
        bytes = golden(11);
        append_tlv(bytes, 0x1234, {});
        append_tlv(bytes, 0x8009, {});
        Message message{};
        check(parse_message(bytes.data(), bytes.size(), Domain, message) != ParseResult::Valid,
              "authentication behind unsupported TLV fails closed");
    }

    void signed_corrections()
    {
        for (int64_t value : {int64_t{0}, int64_t{1}, int64_t{-1}, int64_t{65536}, int64_t{-98304},
                              std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()})
        {
            for (uint8_t type : std::initializer_list<uint8_t>{0, 8, 9, 11})
            {
                auto bytes = golden(type);
                put(bytes, 8, static_cast<uint64_t>(value), 8);
                check(read64(bytes.data() + 8) == static_cast<uint64_t>(value), "unsigned wire correction decoded");
                check(read_signed64(bytes.data() + 8) == value, "signed 64-bit boundary decoded");
                check(parse_valid(bytes).correction == value, "signed correction parsed");
            }
        }
    }

    Packet packet_at(int64_t received_us, uint32_t nanoseconds = 100)
    {
        Packet packet{};
        packet.received_us = received_us;
        packet.hardware_timestamp = true;
        packet.timestamp = {100, nanoseconds};
        return packet;
    }

    void receive_timestamp_policy()
    {
        for (uint8_t type : std::initializer_list<uint8_t>{0, 8, 9, 11})
        {
            const Message message = parse_valid(golden(type, 0));
            Packet packet = packet_at(100);
            check(has_required_rx_timestamp(message, packet), "hardware RX timestamp accepted");
            packet.hardware_timestamp = false;
            packet.timestamp = {};
            check(has_required_rx_timestamp(message, packet) == (type != 0),
                  "only Sync requires hardware RX; Announce, Follow_Up and Delay_Resp do not");
        }

        const Message sync = parse_valid(golden(0, 0));
        Message follow = parse_valid(golden(8, 0));
        follow.timestamp = {100, 0};
        Message response = parse_valid(golden(9, 0));
        response.timestamp = {100, 1200};
        const Packet sync_packet = packet_at(100, 150);
        Packet general_packet{};
        general_packet.received_us = 101;
        std::array<Pair, 4> pairs{};
        Pair completed{};
        check(has_required_rx_timestamp(sync, sync_packet) && has_required_rx_timestamp(follow, general_packet),
              "hardware Sync RX plus untimestamped Follow_Up is valid");
        check(!match_pair(pairs, sync, sync_packet, completed), "hardware Sync waits for untimestamped Follow_Up");
        check(match_pair(pairs, follow, general_packet, completed), "untimestamped Follow_Up completes hardware Sync pair");
        check(completed.reception.hardware_timestamp && completed.reception.timestamp.nanoseconds == 150,
              "pair retains hardware Sync RX, never general-message callback time");
        check(has_required_rx_timestamp(response, general_packet), "untimestamped Delay_Resp accepted");
        check(match_delay_response(response, Master.data(), 0, sync.sequence, Requester.data()),
              "untimestamped Delay_Resp still requires exact exchange identity");
        Measurement measurement{};
        check(calculate_measurement(completed, Timestamp{100, 1150}, response, measurement),
              "measurement uses hardware Sync RX, Delay_Req TX and timestamps carried by general messages");
        check(measurement.mean_path_delay_ns == 100 && measurement.local_minus_master_ns == 50,
              "untimestamped general messages preserve E2E measurement results");
        response.sequence++;
        check(!match_delay_response(response, Master.data(), 0, sync.sequence, Requester.data()),
              "removing general RX timestamp requirement does not weaken response matching");
    }

    void pairing()
    {
        const Message sync = parse_valid(golden(0));
        const Message follow = parse_valid(golden(8));
        Pair completed{};
        std::array<Pair, 4> pairs{};
        check(!match_pair(pairs, sync, packet_at(100), completed), "sync waits for follow-up");
        check(match_pair(pairs, follow, packet_at(101), completed), "ordered pair completed");
        check(completed.sync_seen && completed.follow_seen && completed.sequence == sync.sequence &&
              completed.reception.received_us == 100 && completed.reception.timestamp.nanoseconds == 100,
              "completed pair retains Sync reception");
        check(std::none_of(pairs.begin(), pairs.end(), [](const Pair &pair) { return pair.occupied; }), "completed slot cleared");
        check(!match_pair(pairs, follow, packet_at(200), completed), "follow-up waits for Sync");
        check(match_pair(pairs, sync, packet_at(201, 321), completed), "misordered pair completed");
        check(completed.first_us == 200 && completed.reception.timestamp.nanoseconds == 321, "misordered reception is Sync");
        for (size_t field = 0; field < 4; ++field)
        {
            pairs = {};
            auto other = follow;
            if (field == 0)
                other.source[0] ^= 1;
            else if (field == 1)
                other.source[9] ^= 3;
            else if (field == 2)
                ++other.sequence;
            else
                other.minor_version = 0;
            check(!match_pair(pairs, sync, packet_at(100), completed), "mismatch setup");
            check(!match_pair(pairs, other, packet_at(101), completed), "identity/port/sequence/version mismatch isolated");
            check(match_pair(pairs, follow, packet_at(102), completed), "original key remains matchable");
        }
        for (bool follow_first : {false, true})
        {
            pairs = {};
            check(!match_pair(pairs, follow_first ? follow : sync, packet_at(100), completed), "duplicate setup");
            Message duplicate = follow_first ? follow : sync;
            duplicate.correction = 999;
            check(!match_pair(pairs, duplicate, packet_at(101, 999), completed), "duplicate does not complete");
            check(match_pair(pairs, follow_first ? sync : follow, packet_at(102), completed), "pair after duplicate completes");
            check((follow_first ? completed.follow : completed.sync).correction == 0, "first duplicate data retained");
            if (!follow_first)
                check(completed.reception.received_us == 100, "first Sync reception retained");
        }
        for (bool follow_first : {false, true})
        {
            for (int64_t elapsed : {PairLifetimeUs, PairLifetimeUs + 1})
            {
                pairs = {};
                check(!match_pair(pairs, follow_first ? follow : sync, packet_at(100), completed), "expiration setup");
                check(match_pair(pairs, follow_first ? sync : follow, packet_at(100 + elapsed), completed) ==
                      (elapsed == PairLifetimeUs), "expiration inclusive boundary");
            }
        }
        pairs = {};
        for (uint16_t sequence : {uint16_t{65535}, uint16_t{0}})
            check(!match_pair(pairs, parse_valid(golden(0, 1, sequence)), packet_at(100 + sequence), completed), "wrap Sync pending");
        for (uint16_t sequence : {uint16_t{0}, uint16_t{65535}})
        {
            check(match_pair(pairs, parse_valid(golden(8, 1, sequence)), packet_at(70000), completed), "sequence wrap pairs independently");
            check(completed.sequence == sequence, "wrapped sequence preserved");
        }
        pairs = {};
        for (uint16_t sequence = 1; sequence <= 5; ++sequence)
            check(!match_pair(pairs, parse_valid(golden(0, 1, sequence)), packet_at(sequence), completed), "bounded cache insertion");
        check(std::none_of(pairs.begin(), pairs.end(), [](const Pair &pair) { return pair.sequence == 1; }), "oldest cache entry evicted");
        check(match_pair(pairs, parse_valid(golden(8, 1, 5)), packet_at(6), completed), "newest cache entry retained");
        pairs = {};
        auto one_step = sync;
        one_step.flags = 0;
        check(!match_pair(pairs, one_step, packet_at(1), completed), "one-step Sync ignored");
        check(!match_pair(pairs, parse_valid(golden(9)), packet_at(2), completed), "nonpair message ignored");
        check(std::none_of(pairs.begin(), pairs.end(), [](const Pair &pair) { return pair.occupied; }), "ignored messages do not allocate");
    }

    void delay_exchange()
    {
        const Message response = parse_valid(golden(9));
        check(match_delay_response(response, Master.data(), 1, 0x1234, Requester.data()), "delay response matches");
        for (size_t field = 0; field < 7; ++field)
        {
            auto other = response;
            if (field == 0)
                other.type = 8;
            else if (field == 1)
                other.minor_version = 0;
            else if (field == 2)
                ++other.sequence;
            else if (field == 3)
                other.source[0] ^= 1;
            else if (field == 4)
                other.source[9] ^= 3;
            else if (field == 5)
                other.requester[0] ^= 1;
            else
                other.requester[9] ^= 3;
            check(!match_delay_response(other, Master.data(), 1, 0x1234, Requester.data()), "delay response key mismatch rejected");
        }
        const std::array<uint8_t, 6> mac = {2, 0x11, 0x22, 0x33, 0x44, 0x55};
        for (uint8_t minor : std::initializer_list<uint8_t>{0, 1})
        {
            for (uint16_t sequence : {uint16_t{65535}, uint16_t{0}})
            {
                std::array<uint8_t, 62> guarded{};
                guarded.fill(0xa5);
                make_delay_request(guarded.data() + 1, mac.data(), Requester.data(), Domain, minor, sequence);
                Bytes expected(60, 0);
                const std::array<uint8_t, 6> multicast = {1, 0x1b, 0x19, 0, 0, 0};
                std::copy(multicast.begin(), multicast.end(), expected.begin());
                std::copy(mac.begin(), mac.end(), expected.begin() + 6);
                put(expected, 12, 0x88f7, 2);
                expected[14] = 1;
                expected[15] = static_cast<uint8_t>((minor << 4) | 2);
                put(expected, 16, 44, 2);
                expected[18] = Domain;
                std::copy(Requester.begin(), Requester.end(), expected.begin() + 34);
                put(expected, 44, sequence, 2);
                expected[46] = minor == 0 ? 1 : 0;
                expected[47] = 0x7f;
                check(guarded.front() == 0xa5 && guarded.back() == 0xa5, "request writes exactly 60 bytes");
                check(std::equal(expected.begin(), expected.end(), guarded.begin() + 1), "delay request exact wire image");
                Message request{};
                check(parse_message(guarded.data() + 15, 46, Domain, request) == ParseResult::Valid,
                      "generated request parses with Ethernet padding");
                check(request.sequence == sequence && request.minor_version == minor && request.timestamp.seconds == 0 &&
                      request.timestamp.nanoseconds == 0 && std::equal(Requester.begin(), Requester.end(), request.source),
                      "request fields round trip");
                auto wrapped = response;
                wrapped.sequence = sequence;
                wrapped.minor_version = minor;
                check(match_delay_response(wrapped, Master.data(), minor, sequence, Requester.data()), "delay sequence wrap matches");
            }
        }
    }

    void near(long double actual, long double expected, const std::string &description)
    {
        check(std::fabs(actual - expected) < 0.000001L, description);
    }

    void measurements()
    {
        Pair pair{};
        pair.follow.timestamp = {100, 0};
        pair.reception.timestamp = {100, 150};
        Timestamp transmit{100, 1150};
        Message response{};
        response.timestamp = {100, 1200};
        Measurement result{};
        check(calculate_measurement(pair, transmit, response, result), "uncorrected measurement accepted");
        near(result.mean_path_delay_ns, 100, "uncorrected path delay");
        near(result.local_minus_master_ns, 50, "uncorrected offset");
        pair.sync.correction = 10 * 65536;
        pair.follow.correction = 20 * 65536;
        response.correction = 40 * 65536;
        check(calculate_measurement(pair, transmit, response, result), "corrected measurement accepted");
        near(result.mean_path_delay_ns, 65, "all correction fields subtracted from path");
        near(result.local_minus_master_ns, 55, "Sync and Follow_Up corrections subtracted from offset");
        pair.sync.correction = -32768;
        pair.follow.correction = 16384;
        response.correction = -98304;
        check(calculate_measurement(pair, transmit, response, result), "signed fractional corrections accepted");
        near(result.mean_path_delay_ns, 100.875L, "fractional path delay");
        near(result.local_minus_master_ns, 49.375L, "fractional offset");
        pair = {};
        response = {};
        pair.follow.timestamp = {100, 999999900};
        pair.reception.timestamp = {101, 50};
        transmit = {101, 1050};
        response.timestamp = {101, 1100};
        check(calculate_measurement(pair, transmit, response, result), "second rollover accepted");
        near(result.mean_path_delay_ns, 100, "second rollover path");
        near(result.local_minus_master_ns, 50, "second rollover offset");
        near(difference_ns({100, 999999900}, {101, 50}), -150, "negative second rollover difference");
        for (const auto &elapsed : std::array<std::array<uint64_t, 2>, 6>{{{0, 0}, {2, 0}, {5, 5}, {5, 3}, {6, 5}, {5, 6}}})
        {
            pair = {};
            pair.follow.timestamp = {100, 0};
            pair.reception.timestamp = {100, 0};
            transmit = {100 + elapsed[1], 0};
            response.timestamp = {100 + elapsed[0], 0};
            const bool valid = elapsed[0] <= 5 && elapsed[1] <= 5 && elapsed[0] >= elapsed[1] && elapsed[0] - elapsed[1] <= 2;
            check(calculate_measurement(pair, transmit, response, result) == valid, "elapsed/path inclusive limits");
            if (valid)
                near(result.mean_path_delay_ns, (elapsed[0] - elapsed[1]) * 500000000.0L, "elapsed/path boundary value");
        }
        pair = {};
        pair.follow.timestamp = {100, 100};
        pair.reception.timestamp = {100, 100};
        response.timestamp = {100, 99};
        check(!calculate_measurement(pair, {100, 100}, response, result), "negative master elapsed rejected");
        response.timestamp = {100, 100};
        check(!calculate_measurement(pair, {100, 99}, response, result), "negative local elapsed rejected");
        response.timestamp = {105, 101};
        check(!calculate_measurement(pair, {105, 100}, response, result), "master elapsed one nanosecond over limit");
        response.timestamp = {105, 100};
        check(!calculate_measurement(pair, {105, 101}, response, result), "local elapsed one nanosecond over limit");
        response.timestamp = {102, 101};
        check(!calculate_measurement(pair, {100, 100}, response, result), "path delay over one second rejected");
        response.timestamp = {100, 100};
        pair.sync.correction = 1;
        check(!calculate_measurement(pair, {100, 100}, response, result), "negative fractional path rejected");
        pair.sync.correction = std::numeric_limits<int64_t>::min();
        check(!calculate_measurement(pair, {100, 100}, response, result), "extreme negative correction rejected");
        pair.sync.correction = std::numeric_limits<int64_t>::max();
        check(!calculate_measurement(pair, {100, 100}, response, result), "extreme positive correction rejected");
    }

    void negative_request_builders()
    {
        struct ByteChange
        {
            size_t first;
            uint8_t second;
        };
        struct Fixture
        {
            Mutation mutation;
            const char *name;
            size_t length;
            ParseResult tester_result;
            std::vector<ByteChange> changes;
        };
        const Fixture fixtures[] = {
            {Mutation::WrongDomain, "wrong-domain", 60, ParseResult::Ignore, {{18, Domain + 1}}},
            {Mutation::Version, "version", 60, ParseResult::Unsupported, {{15, 3}}},
            {Mutation::MinorVersion, "minor-version", 60, ParseResult::Valid, {{15, 0x12}, {46, 0}}},
            {Mutation::UnsupportedMinorVersion, "unsupported-minor-version", 60, ParseResult::Unsupported, {{15, 0x22}}},
            {Mutation::Transport, "transportSpecific", 60, ParseResult::Unsupported, {{14, 0x11}}},
            {Mutation::ReservedDomain, "reserved-domain", 60, ParseResult::Unsupported, {{19, 1}}},
            {Mutation::ReservedHeader, "reserved-header", 60, ParseResult::Unsupported, {{30, 1}}},
            {Mutation::Flags, "flags", 60, ParseResult::Valid, {{20, 2}}},
            {Mutation::Control, "control", 60, ParseResult::Malformed, {{46, 5}}},
            {Mutation::ShortHeader, "short-PTP-header", 60, ParseResult::Malformed, {{17, 32}}},
            {Mutation::ShortBody, "short-PTP-body", 60, ParseResult::Malformed, {{17, 42}}},
            {Mutation::InconsistentLength, "messageLength", 60, ParseResult::Malformed, {{17, 64}}},
            {Mutation::Oversized, "oversized", 270, ParseResult::Unsupported, {{16, 1}, {17, 0}}},
            {Mutation::ExtraTlv, "extra-TLV", 62, ParseResult::Unsupported, {{17, 48}, {58, 0x12}, {59, 0x34}}},
            {Mutation::AuthenticationTlv, "authentication-TLV-unsupported", 62, ParseResult::AuthenticationUnsupported,
             {{17, 48}, {58, 0x80}, {59, 9}}},
            {Mutation::TruncatedTlv, "truncated-TLV", 60, ParseResult::Malformed, {{17, 46}, {58, 0x12}}},
            {Mutation::OddTlv, "odd-TLV", 64, ParseResult::Malformed, {{17, 50}, {58, 0x12}, {59, 0x34}, {61, 1}}},
            {Mutation::OverrunningTlv, "overrunning-TLV", 64, ParseResult::Malformed,
             {{17, 50}, {58, 0x12}, {59, 0x34}, {61, 4}}},
            {Mutation::ZeroIdentity, "zero-clock", 60, ParseResult::Malformed,
             {{34, 0}, {35, 0}, {36, 0}, {37, 0}, {38, 0}, {39, 0}, {40, 0}, {41, 0}}},
            {Mutation::OnesIdentity, "ones-clock", 60, ParseResult::Malformed,
             {{34, 255}, {35, 255}, {36, 255}, {37, 255}, {38, 255}, {39, 255}, {40, 255}, {41, 255}}},
            {Mutation::ZeroPort, "zero-port", 60, ParseResult::Malformed, {{43, 0}}},
            {Mutation::ReservedPort, "reserved-port", 60, ParseResult::Malformed, {{42, 255}, {43, 255}}},
            {Mutation::OwnIdentity, "target-clock", 60, ParseResult::Valid,
             {{34, 0x10}, {35, 0x20}, {36, 0x30}, {37, 0xff}, {38, 0xfe}, {39, 0x40}, {40, 0x50}, {41, 0x60}}},
            {Mutation::InvalidNanoseconds, "nanoseconds", 60, ParseResult::Malformed,
             {{54, 0x3b}, {55, 0x9a}, {56, 0xca}, {57, 0}}},
            {Mutation::PositiveCorrection, "positive-correction", 60, ParseResult::Valid, {{29, 1}}},
            {Mutation::NegativeCorrection, "negative-correction", 60, ParseResult::Valid,
             {{22, 255}, {23, 255}, {24, 255}, {25, 255}, {26, 255}, {27, 255}, {28, 255}, {29, 255}}},
            {Mutation::WrongDestination, "destination-filter", 60, ParseResult::Valid, {{0, 3}, {5, 1}}},
            {Mutation::Vlan, "VLAN-filter", 64, ParseResult::Valid, {}},
            {Mutation::Padding, "legal-padding", 60, ParseResult::Valid, {{58, 0xa5}, {59, 0x5a}}},
            {Mutation::SecondPort, "distinct-valid-port", 60, ParseResult::Valid, {{43, 2}}},
        };
        check(std::size(fixtures) == static_cast<size_t>(Mutation::Count), "every mutation has an independent fixture");
        const uint8_t mac[] = {2, 0x11, 0x22, 0x33, 0x44, 0x55};
        auto identity = Requester;
        identity[9] = 1;
        const Bytes baseline = {
            1, 0x1b, 0x19, 0, 0, 0, 2, 0x11, 0x22, 0x33, 0x44, 0x55, 0x88, 0xf7,
            1, 2, 0, 44, Domain, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            2, 0x11, 0x22, 0xff, 0xfe, 0x33, 0x44, 0x55, 0, 1, 0x12, 0x34, 1, 0x7f,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };
        check(baseline.size() == 60, "independent baseline has minimum Ethernet size");
        for (size_t index = 0; index < std::size(fixtures); ++index)
        {
            const Fixture &fixture = fixtures[index];
            const std::string label = fixture.name;
            check(static_cast<size_t>(fixture.mutation) == index && label == MutationNames[index], label + " stable name and enum");
            Bytes expected = baseline;
            if (fixture.mutation == Mutation::Vlan)
                expected.insert(expected.begin() + 12, {0x81, 0, 0, 1});
            expected.resize(fixture.length, 0);
            for (const auto &change : fixture.changes)
                expected.at(change.first) = change.second;
            Bytes guarded(fixture.length + 2, 0);
            guarded.front() = guarded.back() = 0xcd;
            make_delay_request(guarded.data() + 1, mac, identity.data(), Domain, 0, 0x1234);
            check(mutate_request(guarded.data() + 1, fixture.mutation, Master.data()) == fixture.length,
                  label + " exact transmitted length");
            check(guarded.front() == 0xcd && guarded.back() == 0xcd, label + " exact-size buffer guards");
            check(std::equal(expected.begin(), expected.end(), guarded.begin() + 1), label + " independent exact wire image");
            const bool positive = fixture.mutation == Mutation::MinorVersion || fixture.mutation == Mutation::Padding ||
                                  fixture.mutation == Mutation::SecondPort;
            check(mutation_expects_response(fixture.mutation, guarded[16] >> 4) == positive,
                  label + " confirmed firmware response policy");
            const size_t header = fixture.mutation == Mutation::Vlan ? 18 : 14;
            Message message{};
            check(parse_message(guarded.data() + 1 + header, fixture.length - header, Domain, message) == fixture.tester_result,
                  label + " tester parser outcome, not firmware L2 acceptance");
            if (fixture.mutation == Mutation::MinorVersion)
                check(message.minor_version == 1, "supported unsigned v2.1 request must receive a response");
            if (fixture.mutation == Mutation::PositiveCorrection || fixture.mutation == Mutation::NegativeCorrection)
                check(message.correction == (fixture.mutation == Mutation::PositiveCorrection ? 1 : -1),
                      "tester decodes signed corrections; confirmed firmware requires zero Delay_Req correction");
            if (positive)
                check(message.source[9] == (fixture.mutation == Mutation::SecondPort ? 2 : 1) && message.sequence == 0x1234,
                      label + " positive control preserves request correlation");
        }
    }

    void negative_classification()
    {
        for (bool before : {false, true})
            for (bool after : {false, true})
                for (bool response : {false, true})
                    for (bool expected : {false, true})
                        for (bool interrupted : {false, true})
                            for (uint32_t drops : {0U, 1U, std::numeric_limits<uint32_t>::max()})
                            {
                                const Result result = !before || !after || interrupted || drops != 0 ? Result::CouldNotRun :
                                                      response == expected ? Result::Pass : Result::Fail;
                                check(classify_negative(before, after, response, expected, interrupted, drops) == result,
                                      "negative classification requires both probes; interruption/drop overrides pass and fail");
                            }
    }

    void authenticated_negative_request_builders()
    {
        constexpr char Key[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
        Authenticator auth;
        check(auth.initialize(0x1234, Key, 7), "authenticated negative-request key initialized");
        const uint8_t mac[] = {2, 0x11, 0x22, 0x33, 0x44, 0x55};
        size_t supported = 0;
        for (size_t index = 0; index < static_cast<size_t>(Mutation::Count); ++index)
        {
            const Mutation mutation = static_cast<Mutation>(index);
            const bool excluded = mutation == Mutation::Version || mutation == Mutation::MinorVersion ||
                                  mutation == Mutation::UnsupportedMinorVersion ||
                                  mutation == Mutation::ShortHeader || mutation == Mutation::ShortBody ||
                                  mutation == Mutation::InconsistentLength || mutation == Mutation::Oversized ||
                                  mutation == Mutation::ExtraTlv || mutation == Mutation::AuthenticationTlv ||
                                  mutation == Mutation::TruncatedTlv || mutation == Mutation::OddTlv ||
                                  mutation == Mutation::OverrunningTlv || mutation == Mutation::Vlan ||
                                  mutation == Mutation::Padding;
            check(authenticated_mutation_supported(mutation) == !excluded,
                  std::string(MutationNames[index]) + " authenticated coverage boundary");
            if (excluded)
                continue;
            ++supported;
            std::array<uint8_t, MaxFrameSize> frame{};
            auto identity = Requester;
            identity[9] = static_cast<uint8_t>(index + 1);
            make_delay_request(frame.data(), mac, identity.data(), Domain, 1, static_cast<uint16_t>(0x1200 + index));
            const size_t frame_length = mutate_request(frame.data(), mutation, Master.data());
            check(frame_length == 60, std::string(MutationNames[index]) + " retains a fixed-size PTP body");
            const uint8_t control = frame[46];
            promote_to_v21(frame.data() + 14);
            check(frame[46] == control, "signing preserves the deliberately mutated v2.1 control field");
            size_t message_length = 44;
            check(auth.append(frame.data() + 14, sizeof(frame) - 14, message_length),
                  std::string(MutationNames[index]) + " signed after mutation");
            check(message_length == 44 + AuthTlvSize &&
                      auth.verify(frame.data() + 14, message_length) == AuthResult::Valid,
                  std::string(MutationNames[index]) + " retains a valid authenticated PTP request");
            const bool positive = mutation == Mutation::Control || mutation == Mutation::SecondPort;
            check(mutation_expects_response(mutation, frame[15] >> 4) == positive,
                  std::string(MutationNames[index]) + " signed response expectation uses the actual v2.1 version");
            if (mutation == Mutation::Control)
            {
                Message message{};
                check(frame[46] == 5 && parse_message(frame.data() + 14, message_length, Domain, message, &auth) ==
                          ParseResult::Valid && message.authenticated, "signed deprecated-control probe is a positive control");
                check(!mutation_expects_response(mutation, 0), "the same control mutation remains negative in v2.0");
            }
        }
        check(supported == 16, "all and only the sixteen signable PTP-field mutations are covered");
    }

    void foreign_announce_and_properties()
    {
        const uint8_t mac[] = {2, 0x11, 0x22, 0x33, 0x44, 0x55};
        for (uint16_t sequence : {uint16_t{0}, uint16_t{65535}})
        {
            std::array<uint8_t, 80> guarded{};
            guarded.fill(0xcd);
            make_foreign_announce(guarded.data() + 1, mac, Requester.data(), Domain, sequence);
            Bytes expected(78, 0);
            const Bytes prefix = {1, 0x1b, 0x19, 0, 0, 0, 2, 0x11, 0x22, 0x33, 0x44, 0x55, 0x88, 0xf7,
                                  11, 2, 0, 64, Domain};
            std::copy(prefix.begin(), prefix.end(), expected.begin());
            std::copy(Requester.begin(), Requester.end(), expected.begin() + 34);
            put(expected, 44, sequence, 2);
            expected[46] = 5;
            expected[47] = 1;
            expected[61] = 128;
            expected[62] = 248;
            expected[63] = 0xfe;
            expected[64] = expected[65] = 0xff;
            expected[66] = 128;
            std::copy(Requester.begin(), Requester.begin() + 8, expected.begin() + 67);
            expected[77] = 0xa0;
            check(guarded.front() == 0xcd && guarded.back() == 0xcd, "foreign Announce writes exactly 78 bytes");
            check(std::equal(expected.begin(), expected.end(), guarded.begin() + 1), "foreign Announce independent exact bytes");
            Message message{};
            check(parse_message(guarded.data() + 15, 64, Domain, message) == ParseResult::Valid,
                  "foreign Announce tester parser accepts baseline");
            check(conservative_properties(message), "baseline class 248, accuracy fe, variance ffff, source a0, flags clear");
            for (int16_t offset : {int16_t{-123}, int16_t{0}, int16_t{42}, std::numeric_limits<int16_t>::max()})
            {
                message.utc_offset = offset;
                check(conservative_properties(message), "clear UTC-valid flag makes arbitrary UTC offset non-authoritative");
            }
            for (unsigned bit = 0; bit < 6; ++bit)
            {
                auto changed = message;
                changed.flags = static_cast<uint16_t>(1U << bit);
                check(!conservative_properties(changed), "every asserted time-property flag rejects conservative baseline");
            }
            for (size_t field = 0; field < 6; ++field)
            {
                auto changed = message;
                switch (field)
                {
                case 0: changed.type = 0; break;
                case 1: changed.clock_class = 6; break;
                case 2: changed.accuracy = 0x21; break;
                case 3: changed.variance = 0; break;
                case 4: changed.steps = 1; break;
                case 5: changed.time_source = 0x20; break;
                }
                check(!conservative_properties(changed), "each conservative field independently required");
            }
        }
    }

    void restricted_output_policy()
    {
        for (uint8_t type : {uint8_t{0}, uint8_t{8}, uint8_t{9}, uint8_t{11}})
        {
            const Bytes bytes = golden(type);
            Message message = parse_valid(bytes);
            message.minor_version = 0;
            message.correction = 0;
            message.flags = type == 0 ? TwoStepFlag : 0;
            Packet packet{};
            packet.length = static_cast<uint16_t>(14 + bytes.size());
            std::copy(bytes.begin(), bytes.end(), packet.frame + 14);
            check(restricted_output(packet, message), "restricted output accepts exact baseline");
            const uint16_t length = packet.length;
            --packet.length;
            check(!restricted_output(packet, message), "restricted output rejects truncated body");
            packet.length = length;
            ++packet.length;
            check(restricted_output(packet, message), "Ethernet padding does not change declared content");
            ++packet.frame[17];
            check(!restricted_output(packet, message), "restricted output rejects extra declared content");
            --packet.frame[17];
            message.minor_version = 1;
            check(!restricted_output(packet, message), "restricted output rejects minor version one");
            message.minor_version = 0;
            for (int64_t correction : {int64_t{-1}, int64_t{1}})
            {
                message.correction = correction;
                check(!restricted_output(packet, message), "restricted output rejects signed nonzero correction");
            }
            message.correction = 0;
            message.flags ^= TwoStepFlag;
            check(!restricted_output(packet, message), "restricted output requires exact per-type flags");
            message.type = 1;
            check(!restricted_output(packet, message), "transmitter must not emit Delay requests");
        }
    }

    void gnss_tai_announce_policy()
    {
        Bytes bytes = golden(11, 0);
        bytes[48] = 248;
        bytes[49] = 0xfe;
        put(bytes, 50, 65535, 2);
        put(bytes, 61, 0, 2);
        bytes[63] = 0x20;
        Packet packet{};
        packet.length = static_cast<uint16_t>(14 + bytes.size());
        for (uint16_t flags : {uint16_t{0x000c}, uint16_t{0x000d}, uint16_t{0x000e}})
        {
            put(bytes, 6, flags, 2);
            std::copy(bytes.begin(), bytes.end(), packet.frame + 14);
            const Message message = parse_valid(bytes);
            check(!conservative_properties(message), "GNSS PTP/TAI is distinct from legacy arbitrary timescale");
            check(supported_properties(message), "current server GNSS PTP/TAI Announce accepted");
            check(restricted_output(packet, message), "current server Announce wire policy accepted");
            for (uint16_t rejected : {uint16_t{0}, uint16_t{4}, uint16_t{8}, uint16_t{15},
                                      uint16_t{0x1c}, uint16_t{0x2c}, uint16_t{0x020c}})
            {
                auto changed = message;
                changed.flags = rejected;
                check(!supported_properties(changed), "missing timescale/UTC-valid, conflicting leap or traceability flags rejected");
            }
            for (size_t field = 0; field < 6; ++field)
            {
                auto changed = message;
                switch (field)
                {
                case 0: changed.type = 0; break;
                case 1: changed.clock_class = 6; break;
                case 2: changed.accuracy = 0x21; break;
                case 3: changed.variance = 0; break;
                case 4: changed.steps = 1; break;
                case 5: changed.time_source = 0xa0; break;
                }
                check(!supported_properties(changed), "GNSS PTP/TAI policy still checks each clock-quality field");
            }
        }
        put(bytes, 6, 0, 2);
        bytes[63] = 0xa0;
        const Message legacy = parse_valid(bytes);
        check(supported_properties(legacy), "legacy arbitrary-timescale server remains supported");
    }

    void sample_statistics_and_classification()
    {
        Statistics statistics{};
        check(statistics.count == 0 && statistics.minimum == 0 && statistics.maximum == 0 && statistics.mean == 0,
              "empty statistics initialized");
        near(statistics.deviation(), 0, "empty sample deviation");
        statistics.add(-3);
        near(statistics.minimum, -3, "first negative sample minimum");
        near(statistics.maximum, -3, "first negative sample maximum");
        near(statistics.deviation(), 0, "single sample deviation");
        for (long double value : {-1.0L, 1.0L, 3.0L})
            statistics.add(value);
        check(statistics.count == 4, "all statistics samples counted");
        near(statistics.minimum, -3, "sample minimum");
        near(statistics.maximum, 3, "sample maximum");
        near(statistics.mean, 0, "sample mean");
        near(statistics.square_sum, 20, "Welford square sum");
        near(statistics.deviation(), std::sqrt(20.0L / 3), "sample rather than population deviation");
        Statistics stable{};
        for (long double value : {1000000000001.0L, 1000000000002.0L, 1000000000003.0L})
            stable.add(value);
        near(stable.mean, 1000000000002.0L, "large baseline stable mean");
        near(stable.deviation(), 1, "large baseline stable deviation");
        Observation healthy{};
        healthy.pairs = healthy.requests = healthy.responses = 3;
        for (long double value : {10.0L, 20.0L, 30.0L})
        {
            healthy.delay.add(value);
            healthy.offset.add(-value);
        }
        check(classify_observation(healthy, 3, false, 0, false) == Result::Pass, "required samples pass inclusively");
        check(classify_observation(healthy, 4, false, 0, false) == Result::Fail, "insufficient samples fail");
        for (size_t fault = 0; fault < 9; ++fault)
        {
            auto observation = healthy;
            switch (fault)
            {
            case 0: observation.pairs = 2; break;
            case 1: observation.responses = 2; break;
            case 2: observation.delay.count = 2; break;
            case 3: observation.missing = 1; break;
            case 4: observation.sequence_gaps = 1; break;
            case 5: observation.response_loss = 1; break;
            case 6: observation.timestamp_failures = 1; break;
            case 7: observation.unusable = 1; break;
            case 8: break;
            }
            const bool protocol_fault = fault == 8;
            const Result expected = fault == 6 || fault == 7 ? Result::CouldNotRun : Result::Fail;
            check(classify_observation(observation, 3, false, 0, protocol_fault) == expected, "each observation fault classified");
            check(classify_observation(observation, 3, true, 0, protocol_fault) == Result::CouldNotRun,
                  "cancellation takes precedence over observation failure");
            check(classify_observation(observation, 3, false, 1, protocol_fault) == Result::CouldNotRun,
                  "queue drop takes precedence over observation failure");
        }
        check(classify_observation(healthy, 3, true, 0, false) == Result::CouldNotRun, "cancellation overrides healthy samples");
        check(classify_observation(healthy, 3, false, std::numeric_limits<uint32_t>::max(), false) == Result::CouldNotRun,
              "any nonzero drops override healthy samples");
        healthy.duplicates = 5;
        healthy.out_of_order = 2;
        check(classify_observation(healthy, 3, false, 0, false) == Result::Pass,
              "duplicate and recoverable reordering diagnostics do not imply missing samples");
    }

    bool observe(Observation &observation, uint8_t type, uint16_t sequence, int64_t received_us,
                 Pair &completed, uint32_t generation = 0)
    {
        Packet packet = packet_at(received_us);
        packet.generation = generation;
        return observation.accept(parse_valid(golden(type, 1, sequence)), packet, completed);
    }

    void observation_duplicates_and_order()
    {
        for (bool follow_first : {false, true})
        {
            Observation observation{};
            Pair completed{};
            const uint8_t first = follow_first ? 8 : 0;
            const uint8_t second = follow_first ? 0 : 8;
            check(!observe(observation, first, 10, 100, completed), "first half remains pending");
            check(!observe(observation, first, 10, 101, completed), "duplicate before completion ignored");
            check(observation.duplicates == 1 && observation.pairs == 0, "pending duplicate counted separately");
            check(observe(observation, second, 10, 102, completed), "pair completes once after duplicate");
            check(!observe(observation, 0, 10, 103, completed) && !observe(observation, 8, 10, 104, completed),
                  "both halves of completed pair suppressed");
            check(observation.pairs == 1 && observation.duplicates == 3 && observation.unmatched == 0 &&
                  observation.missing == 0 && observation.expired == 0 && observation.sequence_gaps == 0 &&
                  observation.out_of_order == (follow_first ? 1U : 0U), "duplicate and message reordering counters independent");
            check(completed.reception.received_us == (follow_first ? 102 : 100), "duplicates never replace original Sync timestamp");
            observation.expire(200, true);
            check(observation.missing == 0, "completed duplicates leave no pending halves");
        }
        Observation observation{};
        Pair completed{};
        int64_t now = 0;
        for (uint16_t sequence : {uint16_t{65534}, uint16_t{65535}, uint16_t{0}, uint16_t{2}, uint16_t{1}, uint16_t{3}})
        {
            check(!observe(observation, 0, sequence, ++now, completed), "sequence progression Sync pending");
            check(observe(observation, 8, sequence, ++now, completed), "sequence progression pair completes");
        }
        check(observation.pairs == 6 && observation.sequence_gaps == 1 && observation.out_of_order == 1 &&
              observation.last_sequence == 3 && observation.missing == 0, "wrap, gap and late completed pair counted distinctly");
        Observation bounded{};
        for (uint16_t sequence = 0; sequence < 130; ++sequence)
        {
            check(!observe(bounded, 0, sequence, ++now, completed), "bounded duplicate history Sync");
            check(observe(bounded, 8, sequence, ++now, completed), "bounded duplicate history completion");
        }
        check(bounded.history_cursor == 130 && bounded.pairs == 130, "duplicate history wraps without limiting sample count");
        check(!observe(bounded, 0, 129, ++now, completed) && bounded.duplicates == 1, "recent history survives ring wrap");
    }

    void observation_missing_and_expiration()
    {
        Pair completed{};
        for (uint8_t type : std::initializer_list<uint8_t>{0, 8})
        {
            Observation observation{};
            check(!observe(observation, type, 1, 100, completed), "unmatched half pending");
            observation.expire(100 + PairLifetimeUs);
            check(observation.unmatched == 0 && observation.missing == 0 && observation.expired == 0,
                  "exact pair lifetime remains eligible");
            observation.expire(101 + PairLifetimeUs);
            check(observation.unmatched == 1 && observation.missing == 1 && observation.expired == 1 &&
                  observation.out_of_order == 0, "timeout counted separately from reordering");
            observation.expire(102 + PairLifetimeUs, true);
            check(observation.missing == 1, "expired half counted only once");
            Observation unfinished{};
            check(!observe(unfinished, type, 2, 200, completed), "window-ending half pending");
            unfinished.expire(201, true);
            check(unfinished.unmatched == 1 && unfinished.missing == 1 && unfinished.expired == 0 &&
                  unfinished.out_of_order == 0, "window finish is missing and unmatched, not a timeout");
        }
        Observation full{};
        for (uint16_t sequence = 1; sequence <= 5; ++sequence)
            check(!observe(full, 0, sequence, sequence, completed), "fill bounded observation cache");
        check(full.unmatched == 1 && full.expired == 1 && full.missing == 1, "cache eviction counts one lost pair");
        check(observe(full, 8, 5, 6, completed), "retained pair still completes after eviction");
        full.expire(7, true);
        check(full.unmatched == 4 && full.expired == 1 && full.missing == 4 && full.pairs == 1,
              "finish counts remaining halves without recounting eviction");
        Observation arrival{};
        check(!observe(arrival, 0, 1, 100, completed), "arrival expiration setup");
        check(!observe(arrival, 8, 1, 101 + PairLifetimeUs, completed), "late counterpart cannot complete expired pair");
        check(arrival.expired == 1 && arrival.missing == 1 && arrival.pairs == 0, "accept expires stale halves before matching");
    }

    void generations_and_independent_windows()
    {
        const Message sync = parse_valid(golden(0));
        const Message follow = parse_valid(golden(8));
        std::array<Pair, 4> pending{};
        Pair completed{};
        Packet old_packet = packet_at(100);
        old_packet.generation = 7;
        Packet new_packet = packet_at(101);
        new_packet.generation = 8;
        check(!match_pair(pending, sync, old_packet, completed), "old generation Sync pending");
        check(!match_pair(pending, follow, new_packet, completed), "new generation cannot complete old Sync");
        new_packet.received_us = 102;
        check(match_pair(pending, sync, new_packet, completed) && completed.generation == 8 && completed.first_us == 101,
              "new generation completes only with new generation halves");
        Observation first{};
        first.generation = 7;
        check(!observe(first, 0, 1, 100, completed, 7), "first window pending Sync");
        check(!observe(first, 8, 1, 101, completed, 8), "observation rejects changed generation");
        check(first.pairs == 0 && first.duplicates == 0 && first.missing == 0, "generation rejection does not create sample diagnostics");
        check(classify_observation(first, 1, true, 0, false) == Result::CouldNotRun, "link generation interruption is not protocol failure");
        Observation second{};
        second.generation = 8;
        check(!observe(second, 8, 1, 200, completed, 8), "new observation does not inherit old pending Sync");
        check(observe(second, 0, 1, 201, completed, 8), "new observation pairs only its own halves");
        second.expire(202, true);
        first.expire(202, true);
        check(first.missing == 1 && first.pairs == 0 && second.missing == 0 && second.pairs == 1,
              "separate windows have independent pending and missing state");
        Observation third{};
        third.generation = 8;
        check(!observe(third, 0, 1, 300, completed, 8) && observe(third, 8, 1, 301, completed, 8),
              "same completed sequence may be observed in independent window");
        check(third.duplicates == 0 && third.sequence_gaps == 0 && third.history_cursor == 1 && third.delay.count == 0,
              "new window resets duplicate history, sequence tracking and statistics");
        for (int64_t received : {int64_t{99}, int64_t{100}, int64_t{199}, int64_t{200}, int64_t{299}, int64_t{300}})
        {
            check(in_window(received, 100, 200) == (received >= 100 && received < 200), "first half-open independent window");
            check(in_window(received, 200, 300) == (received >= 200 && received < 300), "second half-open independent window");
            check(!(in_window(received, 100, 200) && in_window(received, 200, 300)), "adjacent windows never double count");
        }
        check(!in_window(100, 100, 100) && !in_window(100, 200, 100), "empty and reversed windows exclude packets");
        check(in_window(std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::min(), 0) &&
              !in_window(std::numeric_limits<int64_t>::max(), 0, std::numeric_limits<int64_t>::max()),
              "window comparisons do not overflow at signed boundaries");
    }

    uint32_t random_word(uint32_t &state)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    void malformed_corpus()
    {
        uint32_t state = 0x1588abcd;
        size_t rejected = 0;
        for (size_t iteration = 0; iteration < 2048; ++iteration)
        {
            const uint8_t types[] = {0, 1, 8, 9, 11};
            Bytes bytes = golden(types[iteration % 5]);
            for (size_t mutation = 0; mutation < 4; ++mutation)
                bytes[random_word(state) % bytes.size()] = static_cast<uint8_t>(random_word(state));
            switch (iteration % 4)
            {
            case 0:
                put(bytes, 2, 33, 2);
                break;
            case 1:
                put(bytes, 2, bytes.size() + 2, 2);
                break;
            case 2:
                put(bytes, 2, bytes.size(), 2);
                bytes[4] = Domain;
                bytes[0] = types[iteration % 5];
                bytes[1] = 0x12;
                bytes[5] = 0;
                std::fill(bytes.begin() + 20, bytes.begin() + 28, uint8_t{0});
                break;
            default:
                bytes.resize(random_word(state) % 34);
                break;
            }
            expect(bytes, ParseResult::Malformed, "deterministic structurally malformed corpus rejected");
            ++rejected;
        }
        for (size_t iteration = 0; iteration < 2048; ++iteration)
        {
            Bytes bytes(random_word(state) % 301);
            for (auto &byte : bytes)
                byte = static_cast<uint8_t>(random_word(state));
            Message first{};
            Message second{};
            first.sequence = 1;
            second.sequence = 2;
            const ParseResult result = parse_message(bytes.data(), bytes.size(), Domain, first);
            check(parse_message(bytes.data(), bytes.size(), Domain, second) == result &&
                  first.sequence == second.sequence && first.type == second.type && first.correction == second.correction,
                  "bounded random corpus parses deterministically and resets output");
            check(result != ParseResult::Valid, "fixed-seed random malformed corpus rejected");
            ++rejected;
        }
        std::printf("Corpus: %zu deterministic malformed inputs rejected\n", rejected);
    }
}

namespace
{
    DutPolicy policy_fixture(SelectionMode selection = SelectionMode::Ranked)
    {
        DutPolicy policy{};
        policy.version = 1;
        policy.selection = selection;
        policy.authentication = AuthenticationMode::Disabled;
        policy.capacity = 8;
        policy.domain = Domain;
        policy.log_announce_interval = 1;
        policy.foreign_timeout_ms = 10000;
        policy.qualification_timeout_ms = 3500;
        policy.outgoing_key_id = 1;
        policy.identity = "independent-test-fixture";
        policy.provenance = "host-fixture-not-runtime-attestation";
        return policy;
    }

    void dut_policy_preflight()
    {
        const DutPolicy valid = policy_fixture();
        check(policy_error(valid) == nullptr, "versioned ranked policy accepted");
        check(policy_error(policy_fixture(SelectionMode::Legacy)) == nullptr, "versioned legacy policy accepted");
        DutPolicy invalid = valid;
        invalid.version = 0;
        check(policy_error(invalid) != nullptr, "missing policy cannot silently become legacy");
        invalid = valid;
        invalid.selection = SelectionMode::Unknown;
        check(policy_error(invalid) != nullptr, "unknown selection rejected");
        invalid = valid;
        invalid.authentication = AuthenticationMode::Unknown;
        check(policy_error(invalid) != nullptr, "unknown authentication rejected");
        for (uint8_t capacity : {uint8_t{0}, uint8_t{33}})
        {
            invalid = valid;
            invalid.capacity = capacity;
            check(policy_error(invalid) != nullptr, "bounded foreign capacity");
        }
        invalid = valid;
        invalid.identity = nullptr;
        check(policy_error(invalid) != nullptr, "no anonymous source metadata");
        Config config{};
        config.policy = valid;
        config.domain = Domain;
        check(policy_prerequisite(Test::ForeignAnnounce, config) == nullptr, "valid policy usable without removed image/confirmation controls");
        check(policy_prerequisite(Test::RankingPrecedence, config) == nullptr, "ranked policy permits ranking precedence");
        check(requires_suppression_opt_in(Test::RankingPrecedence) && requires_suppression_opt_in(Test::ForeignAnnounce) &&
              requires_suppression_opt_in(Test::SourceIsolation) && requires_suppression_opt_in(Test::SequencePolicy) &&
              requires_suppression_opt_in(Test::DatasetChanges) && requires_suppression_opt_in(Test::MultipleMasters) &&
              requires_suppression_opt_in(Test::IntervalExpiry) &&
              requires_suppression_opt_in(Test::StandbyRecovery) &&
              !requires_suppression_opt_in(Test::Announce),
              "ranking uses the existing foreign-candidate opt-in and cleanup gates");
        check(policy_prerequisite(Test::IntervalExpiry, config) == nullptr, "ranked interval scenarios use validated policy");
        check(policy_prerequisite(Test::DatasetChanges, config) == nullptr &&
              policy_prerequisite(Test::MultipleMasters, config) == nullptr,
              "ranked dataset and overlapping-master scenarios use validated policy");
        check(policy_prerequisite(Test::StandbyRecovery, config) == nullptr,
              "standby GNSS interaction uses the validated ranked policy");
        config.policy.capacity = 1;
        check(policy_prerequisite(Test::DatasetChanges, config) == nullptr &&
              policy_prerequisite(Test::MultipleMasters, config) != nullptr,
              "one candidate slot supports dataset changes but cannot certify two overlapping qualified masters");
        config.policy.capacity = 2;
        check(policy_prerequisite(Test::MultipleMasters, config) == nullptr, "two slots suffice without capacity stress");
        config.policy.selection = SelectionMode::Legacy;
        check(policy_prerequisite(Test::SequencePolicy, config) == nullptr, "legacy has explicit sequence-refresh expectations");
        check(policy_prerequisite(Test::IntervalExpiry, config) == nullptr, "legacy interval scenarios have fixed-timeout expectations");
        check(policy_prerequisite(Test::DatasetChanges, config) != nullptr &&
              policy_prerequisite(Test::MultipleMasters, config) != nullptr,
              "legacy global suppression has no ranked requalification or independent candidate-lifetime contract");
        check(policy_prerequisite(Test::StandbyRecovery, config) != nullptr,
              "standby interaction never assumes ranked reset semantics on a legacy DUT");
        const char *legacy_ranking = policy_prerequisite(Test::RankingPrecedence, config);
        check(legacy_ranking != nullptr && std::string(legacy_ranking).find("not applicable") != std::string::npos,
              "legacy any-Announce policy is explicitly inapplicable, not a ranking failure");
        config.policy = {};
        check(policy_prerequisite(Test::RankingPrecedence, config) != nullptr, "unknown ranking policy is never assumed");
        check(policy_prerequisite(Test::SequencePolicy, config) != nullptr, "sequence tests never guess an unknown selection policy");
        check(policy_prerequisite(Test::IntervalExpiry, config) != nullptr, "interval tests never guess an unknown selection policy");
        check(policy_prerequisite(Test::DatasetChanges, config) != nullptr &&
              policy_prerequisite(Test::MultipleMasters, config) != nullptr, "new scenarios never guess unknown policy");
        check(policy_prerequisite(Test::StandbyRecovery, config) != nullptr, "standby interaction rejects unknown policy");
        config.policy = valid;
        Message advertised = parse_valid(golden(11, 0));
        advertised.log_interval = 1;
        check(wire_policy_error(config, advertised) == nullptr, "matching on-wire Announce agrees with source policy");
        advertised.log_interval = 2;
        check(wire_policy_error(config, advertised) != nullptr, "stale advertised interval cannot produce a policy pass");
        advertised.log_interval = 1;
        advertised.minor_version = 1;
        check(wire_policy_error(config, advertised) != nullptr, "version contradicting unsigned metadata is inconclusive, not silently verified");
        advertised.minor_version = 0;
        advertised.type = 0;
        check(wire_policy_error(config, advertised) != nullptr, "Sync interval mismatch detected independently of Announce");
        ++config.domain;
        check(policy_prerequisite(Test::ForeignAnnounce, config) != nullptr, "selected domain mismatch cannot pass");
        check(policy_prerequisite(Test::RankingPrecedence, config) != nullptr, "ranking domain mismatch cannot pass");
        check(policy_prerequisite(Test::SequencePolicy, config) != nullptr, "sequence policy requires matching domain");
        check(policy_prerequisite(Test::IntervalExpiry, config) != nullptr, "interval policy requires matching domain");
        check(policy_prerequisite(Test::DatasetChanges, config) != nullptr &&
              policy_prerequisite(Test::MultipleMasters, config) != nullptr, "new scenarios require matching domain");
        check(policy_prerequisite(Test::StandbyRecovery, config) != nullptr, "standby interaction requires matching domain");
        config.domain = Domain;
        for (AuthenticationMode mode : {AuthenticationMode::Mixed, AuthenticationMode::Required})
        {
            config.policy.authentication = mode;
            for (Test test : {Test::ForeignAnnounce, Test::RankingPrecedence, Test::SequencePolicy, Test::IntervalExpiry,
                              Test::DatasetChanges, Test::MultipleMasters, Test::StandbyRecovery, Test::Announce})
                check((policy_prerequisite(test, config) != nullptr) == (mode == AuthenticationMode::Required),
                      "only required policy needs a key before learning effective signed/unsigned service");
            config.key_id = 1;
            config.key_value = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
            check(policy_prerequisite(Test::ForeignAnnounce, config) == nullptr, "configured PTP key permits signed policy tests");
            check(policy_prerequisite(Test::RankingPrecedence, config) == nullptr, "separate PTP key permits signed ranking probes");
            check(policy_prerequisite(Test::SequencePolicy, config) == nullptr, "separate PTP key permits signed sequence probes");
            check(policy_prerequisite(Test::IntervalExpiry, config) == nullptr, "separate PTP key permits signed interval probes");
            check(policy_prerequisite(Test::DatasetChanges, config) == nullptr &&
                  policy_prerequisite(Test::MultipleMasters, config) == nullptr, "new scenarios support mixed and required authentication");
            check(policy_prerequisite(Test::StandbyRecovery, config) == nullptr,
                  "standby interaction signs foreign refreshes under mixed/required policy");
            config.key_id = 0;
            config.key_value = nullptr;
        }
    }

    void independent_ranking_and_builder()
    {
        Message local = parse_valid(golden(11));
        local.clock_class = 248;
        local.accuracy = 0xfe;
        local.variance = 65535;
        local.priority2 = 128;
        Message better{};
        Message worse{};
        check(ranked_dataset(local, true, better), "strictly better dataset generated");
        check(better.priority1 == 127, "default priority 128 yields exactly 127");
        check(std::string(compare_rank(better, local).field) == "priority1", "independent rank evidence identifies deciding field");
        check(ranked_dataset(local, false, worse) && worse.priority1 == 129, "inferior dataset generated independently");
        check(ranked_source_dataset(local, true, Requester.data(), better) && better.priority1 == 127 &&
              std::memcmp(better.grandmaster, Requester.data(), 8) == 0,
              "rejected controls are demonstrably better candidates with phase-specific synthetic grandmasters");
        check(ranked_source_dataset(local, true, local.grandmaster, better) &&
              std::memcmp(better.grandmaster, local.grandmaster, 8) != 0,
              "source-derived grandmaster collision cannot turn a better-master control into ignored self reflection");
        check(ranked_dataset(local, true, better, true) && compare_rank(better, local).order < 0, "winning GM identity tie-break");
        check(ranked_dataset(local, false, worse, true) && compare_rank(worse, local).order > 0, "losing GM identity tie-break");
        check(better.priority1 == local.priority1 && better.clock_class == local.clock_class, "identity-only comparisons preserve numeric dataset");
        for (unsigned field = 0; field < 6; ++field)
        {
            Message left = local;
            switch (field)
            {
            case 0: --left.priority1; break;
            case 1: --left.clock_class; break;
            case 2: left.accuracy = 0x31; break;
            case 3: --left.variance; break;
            case 4: --left.priority2; break;
            case 5: check(change_identity(left.grandmaster, true), "identity decrement possible"); break;
            }
            check(compare_rank(left, local).order < 0 && compare_rank(local, left).order > 0, "both comparison directions");
        }
        Message earlier = local;
        --earlier.priority1;
        earlier.priority2 = 255;
        check(compare_rank(earlier, local).order < 0, "earlier field wins despite worse later fields");
        Message boundary = local;
        boundary.priority1 = 0;
        check(ranked_dataset(boundary, true, better) && better.priority1 == 0 && better.clock_class == 1,
              "zero priority never underflows; next supported deciding field used");
        boundary.clock_class = 1;
        boundary.accuracy = 0x20;
        boundary.variance = 0;
        boundary.priority2 = 0;
        std::fill(std::begin(boundary.grandmaster), std::end(boundary.grandmaster), uint8_t{0});
        boundary.grandmaster[7] = 1;
        check(!ranked_dataset(boundary, true, better), "absolute supported minimum reports unavailable instead of wrapping");
        const uint8_t mac[] = {2, 3, 4, 5, 6, 7};
        std::array<uint8_t, 80> frame{};
        frame.front() = frame.back() = 0xcd;
        make_foreign_announce(frame.data() + 1, mac, Requester.data(), Domain, 65535);
        check(ranked_dataset(local, true, better), "wire fixture rank generated");
        set_foreign_dataset(frame.data() + 1, better, -7);
        const uint8_t *message = frame.data() + 15;
        check(message[33] == 0xf9 && message[47] == 127 && message[48] == 248 &&
              message[49] == 0xfe && message[50] == 255 && message[51] == 255 &&
              message[52] == 128 && read16(message + 30) == 65535, "independent Announce field bytes");
        check(std::memcmp(message + 53, better.grandmaster, 8) == 0, "explicit grandmaster on wire");
        check(frame.front() == 0xcd && frame.back() == 0xcd, "dataset builder bounded to Announce frame");
    }

    void dataset_change_fixtures()
    {
        const Message local = parse_valid(golden(11, 0));
        Message original{};
        Message changed{};
        check(ranked_source_dataset(local, true, Requester.data(), original) &&
              changed_grandmaster_dataset(local, original, changed), "two non-self strictly better datasets available");
        check(original.priority1 == 127 && changed.priority1 == 127 &&
              std::memcmp(original.grandmaster, Requester.data(), 8) == 0 &&
              changed.grandmaster[7] == 0x54, "exact independent same-source GM A/B fixture");
        Message reflected = original;
        std::memcpy(reflected.grandmaster, local.grandmaster, 8);
        check(reflected.priority1 < local.priority1 && compare_rank(reflected, local).order < 0,
              "self-GM rejection cannot be mistaken for rejection of an inferior candidate");
        for (AuthenticationMode authentication : {AuthenticationMode::Disabled, AuthenticationMode::Mixed,
                                                   AuthenticationMode::Required})
        {
            Config config{};
            config.policy = policy_fixture();
            config.policy.authentication = authentication;
            config.policy.spp = 1;
            config.domain = Domain;
            Authenticator auth;
            check(auth.initialize(1, "0123456789ABCDEFGHIJKLMNOPQRSTUV", 1), "synthetic dataset signing fixture");
            uint16_t sequence = 65533;
            for (const Message &dataset : {original, changed, reflected})
            {
                Packet packet{};
                make_foreign_announce(packet.frame, Requester.data(), Requester.data(), Domain, ++sequence);
                set_foreign_dataset(packet.frame, dataset, 3);
                check(std::memcmp(packet.frame + 34, Requester.data(), 10) == 0 &&
                      packet.frame[61] == 127 && packet.frame[47] == 3 &&
                      std::memcmp(packet.frame + 67, dataset.grandmaster, 8) == 0,
                      "only GM changes: exact source, rank, advertised interval and GM wire offsets");
                size_t length = 64;
                if (authentication != AuthenticationMode::Disabled)
                {
                    packet.frame[15] = 0x12;
                    check(auth.append(packet.frame + 14, MaxMessageSize, length) &&
                          auth.verify(packet.frame + 14, length) == AuthResult::Valid,
                          "changed and self-reflected grandmasters retain valid authentication");
                }
                packet.length = static_cast<uint16_t>(14 + length);
                Message decoded{};
                check(parse_config_message(packet, config, auth, decoded) == ParseResult::Valid &&
                      decoded.sequence == sequence && decoded.priority1 == 127 &&
                      std::memcmp(decoded.source, Requester.data(), 10) == 0 &&
                      std::memcmp(decoded.grandmaster, dataset.grandmaster, 8) == 0,
                      "all three dataset probes decode without accidentally changing the transmitter identity");
            }
            check(sequence == 0, "dataset changes keep fresh source sequences through rollover");
        }
        Message minimum = local;
        minimum.priority1 = minimum.priority2 = 0;
        minimum.clock_class = 1;
        minimum.accuracy = 0x20;
        minimum.variance = 0;
        std::memset(minimum.grandmaster, 0, 8);
        minimum.grandmaster[7] = 2;
        check(ranked_dataset(minimum, true, original) && original.grandmaster[7] == 1 &&
              !changed_grandmaster_dataset(minimum, original, changed),
              "single better identity at the minimum cannot fabricate a second non-self master");
        Message upper = local;
        upper.priority1 = 127;
        std::memset(upper.grandmaster, 0xff, 8);
        upper.grandmaster[7] = 0xfe;
        check(changed_grandmaster_dataset(local, upper, changed) && changed.grandmaster[7] == 0xfd,
              "upper identity boundary never wraps to an invalid all-ones GM");
    }

    void dataset_and_overlap_timing()
    {
        Config config{};
        config.policy = policy_fixture();
        IntervalTiming timing{};
        int8_t logarithm = 0;
        check(selection_timing(config, logarithm, timing) && logarithm == 3 &&
              timing.check_us == 1250000 && timing.recovery_us == 6000000,
              "default new scenarios select a decisive 24-second candidate lifetime");
        const int64_t lifetime = candidate_lifetime_us(config.policy, logarithm);
        check(lifetime == 24000000, "exact selected lifetime, not eventual-recovery-only testing");
        ForeignSchedule schedule{SelectionMode::Ranked, lifetime, 250000};
        AnnounceSequence sequences{65533};
        schedule.sent(1000000, 1000100);
        sequences.next();
        schedule.sent(2250000, 2250100);
        sequences.next();
        const int64_t old_expiry = schedule.no_early_service_until();
        schedule = {SelectionMode::Ranked, schedule.lifetime_us, schedule.allowance_us};
        schedule.sent(4000000, 4000100);
        check(sequences.next() == 0 && schedule.fresh_sends == 1 && schedule.silence_after_us == 0,
              "changed GM starts independent qualification without resetting the fresh source sequence");
        check(4000000 + timing.recovery_us + timing.check_us + 2 * schedule.allowance_us < old_expiry,
              "release and healthy continuity must precede expiry of the original qualified GM");
        schedule.sent(12000000, 12000100);
        check(schedule.silence_after_us == 12250100 && schedule.no_early_service_until() == 36000000,
              "only changed GM's second Announce starts withdrawal and its own independent lifetime");

        Message best{};
        Message worse{};
        const Message local = parse_valid(golden(11, 0));
        check(ranked_source_dataset(local, true, Requester.data(), best) &&
              changed_grandmaster_dataset(local, best, worse), "overlapping candidate datasets available");
        if (compare_rank(best, worse).order > 0)
            std::swap(best, worse);
        check(best.priority1 == 127 && worse.priority1 == 127 && best.grandmaster[7] == 0x54 &&
              worse.grandmaster[7] == 0x55, "independently known strict best/worse ordering; both better than local");
        for (bool best_expires_first : {true, false})
        {
            ForeignSchedule early{SelectionMode::Ranked, lifetime, 250000};
            ForeignSchedule late{SelectionMode::Ranked, lifetime, 250000};
            early.sent(1000000, 1000100);
            early.sent(2250000, 2250100);
            late.sent(4000000, 4000100);
            late.sent(5250000, 5250100);
            early.sent(5500000, 5500100);
            const int64_t original_early_expiry = early.no_early_service_until();
            const int64_t refresh_at = early.last_end_us + lifetime / 2;
            late.sent(refresh_at, refresh_at + 100);
            const int64_t checkpoint = early.cleanup_until() + timing.check_us;
            check(early.no_early_service_until() == original_early_expiry &&
                  early.no_early_service_until() == 29500000 && late.no_early_service_until() == 41500100,
                  "refreshing one source cannot move the other candidate's expiry");
            check(checkpoint == 31000100 && checkpoint + late.allowance_us < late.no_early_service_until(),
                  "silence is checked after definite first expiry and before even earliest second expiry");
            const Message &first = best_expires_first ? best : worse;
            const Message &second = best_expires_first ? worse : best;
            check((compare_rank(first, second).order < 0) == best_expires_first,
                  "both strict rank/expiry orders are exercised, not two copies of the same scenario");
            for (AuthenticationMode authentication : {AuthenticationMode::Disabled, AuthenticationMode::Mixed,
                                                       AuthenticationMode::Required})
            {
                config.domain = Domain;
                config.policy.authentication = authentication;
                config.policy.spp = 1;
                Authenticator auth;
                check(auth.initialize(1, "0123456789ABCDEFGHIJKLMNOPQRSTUV", 1), "synthetic overlapping-master signing fixture");
                for (unsigned index = 0; index < 2; ++index)
                {
                    std::array<uint8_t, 10> source = Requester;
                    source[7] = static_cast<uint8_t>(source[7] + index);
                    const Message &dataset = index == 0 ? first : second;
                    Packet packet{};
                    make_foreign_announce(packet.frame, Requester.data(), source.data(), Domain, 10);
                    set_foreign_dataset(packet.frame, dataset, 3);
                    size_t length = 64;
                    if (authentication != AuthenticationMode::Disabled)
                    {
                        packet.frame[15] = 0x12;
                        check(auth.append(packet.frame + 14, MaxMessageSize, length) &&
                              auth.verify(packet.frame + 14, length) == AuthResult::Valid,
                              "each independently ranked source is correctly signed");
                    }
                    packet.length = static_cast<uint16_t>(14 + length);
                    Message decoded{};
                    check(parse_config_message(packet, config, auth, decoded) == ParseResult::Valid &&
                          std::memcmp(decoded.source, source.data(), 10) == 0 &&
                          std::memcmp(decoded.grandmaster, dataset.grandmaster, 8) == 0 &&
                          decoded.priority1 == 127 && decoded.log_interval == 3,
                          "both expiry orders retain distinct source identities and strictly better rank on wire");
                }
            }
            CleanupLease lease{};
            lease.attempted(config, late.last_end_us, lifetime, late.allowance_us);
            lease.attempted(config, early.last_end_us, lifetime, early.allowance_us);
            check(lease.deadline_us == late.cleanup_until(),
                  "cleanup covers every overlapping candidate even when last recorded lease expires earlier");
        }
        config.policy.log_announce_interval = config.policy.log_sync_interval = 4;
        check(!selection_timing(config, logarithm, timing), "slow service without decisive release windows is unavailable");
        config.policy.foreign_timeout_ms = 120000;
        check(selection_timing(config, logarithm, timing), "longer fixed policy lifetime can separate slow-service windows");
        config.policy = policy_fixture();
        config.processing_allowance_ms = config.request_interval_ms = config.response_window_ms = 5000;
        check(!selection_timing(config, logarithm, timing), "extreme timing cannot produce a silence-only vacuous pass");
        config.policy.log_sync_interval = 5;
        check(!selection_timing(config, logarithm, timing), "unsupported advertised DUT timing is never assumed");
    }

    void standby_timing_and_refresh()
    {
        Config config{};
        config.policy = policy_fixture();
        IntervalTiming timing{};
        check(standby_timing(config, timing) && timing.check_us == 1250000 && timing.recovery_us == 6000000,
              "default standby configuration has decisive qualification and recovery windows");
        int64_t refresh = 0;
        check(interval_us(StandbyLogInterval, refresh) && refresh == 500000 &&
              candidate_lifetime_us(config.policy, StandbyLogInterval) == 10000000,
              "half-second fresh foreign refreshes retain the ten-second timeout floor");
        config.policy.log_announce_interval = 0;
        check(standby_timing(config, timing), "two half-second refreshes fit inside minimum two-second startup listening");
        config.processing_allowance_ms = 450;
        check(!standby_timing(config, timing), "equality at the requalification/listening boundary is inconclusive");
        config.processing_allowance_ms = 449;
        check(standby_timing(config, timing), "strictly separated requalification/listening boundary is supported");
        config.policy.foreign_timeout_ms = 1000;
        check(!standby_timing(config, timing), "first-Announce controls cannot outlive the candidate lifetime");
        config.policy.log_sync_interval = 5;
        check(!standby_timing(config, timing), "unsupported service timing cannot silently default");

        config = {};
        config.policy = policy_fixture();
        ForeignSchedule schedule{SelectionMode::Ranked, 10000000, 250000};
        schedule.sent(1000000, 1000100);
        schedule.sent(2250000, 2250100);
        check(standby_poll_valid(3500000, 0, schedule), "first poll starts with a qualified live candidate");
        check(standby_poll_valid(3600000, 3500000, schedule), "exact maximum polling gap is supported");
        check(!standby_poll_valid(3600001, 3500000, schedule), "excess polling gap cannot claim continuous standby");
        check(!standby_poll_valid(3499999, 3500000, schedule), "backward monotonic time cannot claim continuity");
        check(!standby_poll_valid(12000000, 0, schedule), "candidate expiry inside delivery uncertainty is inconclusive");
        check(standby_poll_valid(11999999, 0, schedule), "strict pre-expiry margin is checked, not eventual refresh");
        AnnounceSequence sequence{65534};
        CleanupLease lease{};
        int64_t now = 3500000;
        int64_t last_poll = 0;
        constexpr int64_t Duration = 1200000000;
        for (; now < 3500000 + Duration; now += StandbyPollBudgetUs)
        {
            check(standby_poll_valid(now, last_poll, schedule), "two ten-minute prompts preserve a continuously live lease");
            if ((now - 3500000) % refresh == 0)
            {
                sequence.next();
                schedule.sent(now, now + 100);
                lease.attempted(config, now + 100, schedule.lifetime_us, schedule.allowance_us);
            }
            last_poll = now;
        }
        check(sequence.value == 2398 && schedule.fresh_sends == 2402,
              "long prompts retain fresh source sequences across wrap without per-refresh evidence growth");
        check(lease.pending && lease.deadline_us == schedule.cleanup_until(),
              "all prompt refreshes reserve cancellation cleanup through the final candidate expiry");
        check(classify_foreign(Result::Fail, true, false, false) == Result::Fail &&
              classify_foreign(Result::Pass, false, false, false) == Result::CouldNotRun &&
              classify_foreign(Result::Pass, true, false, true) == Result::CouldNotRun,
              "standby violations fail only with healthy cleanup; unavailable recovery and cancellation never pass");
    }

    void additional_selection_and_authentication_fixtures()
    {
        Config config{};
        config.policy = policy_fixture();
        config.domain = Domain;
        config.policy.authentication = AuthenticationMode::Mixed;
        config.key_id = 1;
        config.key_value = "abcdefghijklmnopqrstuvwxyz123456";
        for (SelectionMode mode : {SelectionMode::Ranked, SelectionMode::Legacy})
        {
            config.policy.selection = mode;
            int8_t logarithm = 0;
            IntervalTiming timing{};
            check(invalid_refresh_timing(config, logarithm, timing) && logarithm == 1,
                  "default legacy and ranked policies separate original expiry from invalid refresh");
            const int64_t lifetime = candidate_lifetime_us(config.policy, logarithm);
            const int64_t allowance = static_cast<int64_t>(config.processing_allowance_ms) * 1000;
            const int64_t invalid_offset = timing.recovery_us + timing.check_us + 2 * allowance;
            check(invalid_offset + timing.check_us < lifetime &&
                  lifetime + allowance + timing.recovery_us < invalid_offset + lifetime,
                  "invalid updates occur before original expiry and their possible expiry follows the recovery deadline");
            check(policy_prerequisite(Test::AnnounceAuthenticationPolicy, config) == nullptr &&
                  policy_prerequisite(Test::InvalidAuthenticationRefresh, config) == nullptr,
                  "keyed mixed policy supports both Announce authentication phases in either selection mode");
        }
        config.policy.selection = SelectionMode::Ranked;
        config.policy.authentication = AuthenticationMode::Required;
        check(policy_prerequisite(Test::AnnounceAuthenticationPolicy, config) == nullptr &&
              policy_prerequisite(Test::InvalidAuthenticationRefresh, config) == nullptr,
              "keyed required policy supports rejection and invalid-refresh phases");
        config.key_value = nullptr;
        check(policy_prerequisite(Test::AnnounceAuthenticationPolicy, config) != nullptr &&
              policy_prerequisite(Test::KeyRotation, config) != nullptr,
              "required authentication never substitutes a missing profile key");
        config.key_value = "abcdefghijklmnopqrstuvwxyz123456";
        config.policy.selection = SelectionMode::Legacy;
        for (Test test : {Test::ForeignCapacity, Test::SelectionNtpLoad, Test::StandbyPpsRecovery,
                          Test::StandbyLinkRecovery, Test::StandbyRestartRecovery, Test::StartupListening,
                          Test::StandbyExpiryUnqualified, Test::SelectionNtpHoldover, Test::SelectionNtpPolicyLoad})
            check(policy_prerequisite(test, config) != nullptr,
                  "ranked-only capacity, load and standby scenarios do not guess legacy behavior");
        config.policy.selection = SelectionMode::Ranked;
        for (Test test : {Test::StandbyRecovery, Test::StandbyPpsRecovery, Test::StandbyLinkRecovery,
                          Test::StandbyRestartRecovery, Test::StandbyExpiryUnqualified, Test::SelectionNtpHoldover})
            check(is_standby_recovery(test) && requires_operator(test) && requires_suppression_opt_in(test),
                  "all standby fault interactions require operator assistance and foreign-master authorization");
        check(involves_ntp(Test::SelectionNtpLoad) && requires_suppression_opt_in(Test::SelectionNtpLoad) &&
              compatible_with_firmware_mode(Test::SelectionNtpLoad, FirmwareMode::Combined) &&
              !compatible_with_firmware_mode(Test::SelectionNtpLoad, FirmwareMode::PtpOnly),
              "selection-aware NTP load requires combined firmware and disruptive opt-in");
        check(involves_ntp(Test::SelectionNtpHoldover) && requires_operator(Test::SelectionNtpHoldover) &&
              requires_suppression_opt_in(Test::SelectionNtpHoldover) &&
              compatible_with_firmware_mode(Test::SelectionNtpHoldover, FirmwareMode::Combined) &&
              !compatible_with_firmware_mode(Test::SelectionNtpHoldover, FirmwareMode::PtpOnly),
              "selection-aware NTP holdover requires combined firmware and all assisted safety opt-ins");
        check(involves_ntp(Test::SelectionNtpPolicyLoad) && !requires_operator(Test::SelectionNtpPolicyLoad) &&
              requires_suppression_opt_in(Test::SelectionNtpPolicyLoad) &&
              compatible_with_firmware_mode(Test::SelectionNtpPolicyLoad, FirmwareMode::Combined) &&
              !compatible_with_firmware_mode(Test::SelectionNtpPolicyLoad, FirmwareMode::PtpOnly),
              "selection-aware NTP key-policy probes require bounded combined load and foreign-master opt-in");
        check(requires_operator(Test::StartupListening) && !requires_suppression_opt_in(Test::StartupListening),
              "instrumented startup requires operator confirmation but does not inject a foreign candidate");

        Authenticator auth;
        check(auth.initialize(1, config.key_value, config.policy.spp), "fixture signer initializes");
        const uint8_t mac[6] = {2, 1, 2, 3, 4, 5};
        Message local = parse_valid(golden(11, 0));
        Message better{};
        check(ranked_source_dataset(local, true, Requester.data(), better), "better live-candidate fixture is available");
        uint8_t original[78]{};
        make_foreign_announce(original, mac, Requester.data(), Domain, 65535);
        set_foreign_dataset(original, better, 1);
        for (AuthMutation mutation : {AuthMutation::Icv, AuthMutation::UnknownKey})
        {
            uint8_t frame[MaxFrameSize]{};
            std::memcpy(frame, original, sizeof(original));
            size_t length = 0;
            check(make_invalid_announce_update(frame, sizeof(frame), length, 0, auth, mutation) &&
                  length == 14 + 64 + AuthTlvSize && read16(frame + 44) == 0,
                  "same-source invalid update uses a fresh rollover sequence and exact signed length");
            check(std::memcmp(frame + 34, original + 34, 10) == 0 &&
                  std::memcmp(frame + 67, original + 67, 8) == 0,
                  "invalid refresh retains source and grandmaster identity");
            check(auth.verify(frame + 14, length - 14) ==
                      (mutation == AuthMutation::Icv ? AuthResult::InvalidMac : AuthResult::UnknownKey),
                  "independent signer rejects each invalid update rather than accepting it as unsigned");
            Message parsed{};
            check(parse_message(frame + 14, length - 14, Domain, parsed, &auth) == ParseResult::AuthenticationInvalid,
                  "invalid same-source update is not a valid authenticated Announce");
        }
        size_t length = 0;
        uint8_t short_frame[78]{};
        std::memcpy(short_frame, original, sizeof(original));
        check(!make_invalid_announce_update(short_frame, sizeof(short_frame), length, 1, auth, AuthMutation::Icv),
              "insufficient mutation capacity is rejected before signing");
        config.processing_allowance_ms = config.request_interval_ms = config.response_window_ms = 5000;
        config.policy.foreign_timeout_ms = 1000;
        int8_t logarithm = 0;
        IntervalTiming timing{};
        check(!invalid_refresh_timing(config, logarithm, timing),
              "overlapping timing windows are inconclusive rather than a false pass");
    }

    void startup_reference_classification()
    {
        constexpr int64_t Earliest = 1000000;
        constexpr int64_t Latest = 1400000;
        constexpr int64_t Listening = 4000000;
        check(classify_startup_reference(Earliest, Latest, Listening, 0, true, false) == Result::Pass,
              "continuous silence through the latest possible listening deadline passes with healthy recovery");
        check(classify_startup_reference(Earliest, Latest, Listening, Earliest, true, false) == Result::Fail &&
              classify_startup_reference(Earliest, Latest, Listening, Earliest + Listening - 1, true, false) == Result::Fail,
              "service before the earliest possible listening deadline is definitely premature");
        check(classify_startup_reference(Earliest, Latest, Listening, Earliest + Listening, true, false) == Result::CouldNotRun &&
              classify_startup_reference(Earliest, Latest, Listening, Latest + Listening - 1, true, false) == Result::CouldNotRun,
              "service within worker-start uncertainty is inconclusive, not a false failure");
        check(classify_startup_reference(Earliest, Latest, Listening, Latest + Listening, true, false) == Result::Pass,
              "service at the latest possible deadline is not early");
        check(classify_startup_reference(Earliest, Latest, Listening, 0, false, false) == Result::CouldNotRun &&
              classify_startup_reference(Earliest, Latest, Listening, 0, true, true) == Result::CouldNotRun &&
              classify_startup_reference(Latest, Earliest, Listening, 0, true, false) == Result::CouldNotRun &&
              classify_startup_reference(Earliest, Latest, Listening, Earliest - 1, true, false) == Result::CouldNotRun,
              "missing post-controls, interruption and invalid reference bounds never pass");
    }

    void precedence_matrix_and_boundaries()
    {
        const Message local = parse_valid(golden(11));
        const std::array<std::array<uint32_t, 5>, 6> winning = {{
            {{127, 254, 0xfe, 65535, 255}},
            {{128, 5, 0xfe, 65535, 255}},
            {{128, 6, 0x20, 65535, 255}},
            {{128, 6, 0x21, 0x1233, 255}},
            {{128, 6, 0x21, 0x1234, 128}},
            {{128, 6, 0x21, 0x1234, 129}},
        }};
        const std::array<std::array<uint32_t, 5>, 6> losing = {{
            {{129, 1, 0x20, 0, 0}},
            {{128, 7, 0x20, 0, 0}},
            {{128, 6, 0x22, 0, 0}},
            {{128, 6, 0x21, 0x1235, 0}},
            {{128, 6, 0x21, 0x1234, 130}},
            {{128, 6, 0x21, 0x1234, 129}},
        }};
        const uint8_t mac[] = {2, 3, 4, 5, 6, 7};
        size_t exercised = 0;
        for (size_t field = 0; field < RankFieldCount; ++field)
        {
            for (bool better : {true, false})
            {
                Message candidate{};
                check(precedence_dataset(local, field, better, candidate), "interior baseline supports all twelve directions");
                const auto &expected = better ? winning[field] : losing[field];
                const std::array<uint32_t, 5> actual = {{
                    candidate.priority1, candidate.clock_class, candidate.accuracy, candidate.variance, candidate.priority2
                }};
                check(actual == expected, "independent numeric fixture ties earlier fields and opposes all later fields");
                uint8_t identity[8];
                if (field == 5)
                {
                    std::memcpy(identity, local.grandmaster, sizeof(identity));
                    identity[7] = static_cast<uint8_t>(better ? 0x5f : 0x61);
                }
                else
                {
                    std::memset(identity, better ? 0xff : 0, sizeof(identity));
                    identity[7] = better ? 0xfe : 1;
                }
                check(std::memcmp(candidate.grandmaster, identity, sizeof(identity)) == 0,
                      "independent identity fixture opposes numeric deciding field or breaks final tie");
                check(std::memcmp(candidate.grandmaster, local.grandmaster, sizeof(identity)) != 0,
                      "ranking fixture is never a self-grandmaster reflection");
                const Rank rank = compare_rank(candidate, local);
                check(rank.order == (better ? -1 : 1) && std::string(rank.field) == RankFields[field],
                      "comparator agrees with independent field/direction expectation");
                std::array<uint8_t, 80> frame{};
                frame.front() = frame.back() = 0xcd;
                make_foreign_announce(frame.data() + 1, mac, Requester.data(), Domain, 65535);
                set_foreign_dataset(frame.data() + 1, candidate, 1);
                const uint8_t *wire = frame.data() + 15;
                check(wire[47] == expected[0] && wire[48] == expected[1] && wire[49] == expected[2] &&
                      read16(wire + 50) == expected[3] && wire[52] == expected[4] &&
                      std::memcmp(wire + 53, identity, sizeof(identity)) == 0,
                      "all six independent ranking fixtures survive exact Announce wire serialization");
                Message decoded{};
                check(parse_message(wire, 64, Domain, decoded) == ParseResult::Valid,
                      "every precedence fixture is a supported Announce, not a rejection probe");
                check(frame.front() == 0xcd && frame.back() == 0xcd && read16(wire + 30) == 65535 && wire[33] == 1,
                      "ranking wire construction preserves bounds, sequence and advertised interval");
                ++exercised;
            }
        }
        check(exercised == 12, "interior dataset yields exactly twelve executable ranking directions");

        Message minimum = local;
        minimum.priority1 = minimum.priority2 = 0;
        minimum.clock_class = 1;
        minimum.accuracy = 0x20;
        minimum.variance = 0;
        std::memset(minimum.grandmaster, 0, sizeof(minimum.grandmaster));
        minimum.grandmaster[7] = 1;
        Message maximum = local;
        maximum.priority1 = maximum.priority2 = 255;
        maximum.clock_class = 254;
        maximum.accuracy = 0xfe;
        maximum.variance = 65535;
        std::memset(maximum.grandmaster, 0xff, sizeof(maximum.grandmaster));
        maximum.grandmaster[7] = 0xfe;
        for (size_t field = 0; field < RankFieldCount; ++field)
        {
            Message candidate{};
            check(!precedence_dataset(minimum, field, true, candidate), "supported minimum is N/A, never underflow");
            check(!precedence_dataset(maximum, field, false, candidate), "supported maximum is N/A, never overflow");
            check(precedence_dataset(minimum, field, false, candidate) && valid_identity(candidate.grandmaster) &&
                  std::memcmp(candidate.grandmaster, minimum.grandmaster, 8) != 0,
                  "losing fixture at minimum has a distinct valid grandmaster even when opposition is impossible");
            check(precedence_dataset(maximum, field, true, candidate) && valid_identity(candidate.grandmaster) &&
                  std::memcmp(candidate.grandmaster, maximum.grandmaster, 8) != 0,
                  "winning fixture at maximum has a distinct valid grandmaster even when opposition is impossible");
        }
        Message candidate{};
        Message accuracy = local;
        accuracy.accuracy = 0x31;
        check(precedence_dataset(accuracy, 2, false, candidate) && candidate.accuracy == 0xfe,
              "accuracy skips unsupported codes above 0x31");
        accuracy.accuracy = 0xfe;
        check(precedence_dataset(accuracy, 2, true, candidate) && candidate.accuracy == 0x31,
              "unknown accuracy steps to the largest supported known code");
        check(!precedence_dataset(local, RankFieldCount, true, candidate), "unknown deciding field rejected");

        Message reference = local;
        reference.clock_class = 248;
        reference.accuracy = 0xfe;
        reference.variance = 65535;
        size_t feasible = 0;
        size_t boundaries = 0;
        for (size_t field = 0; field < RankFieldCount; ++field)
            for (bool better : {true, false})
            {
                const bool available = precedence_dataset(reference, field, better, candidate);
                check(available == (better || (field != 2 && field != 3)),
                      "reference DUT only lacks losing accuracy and losing variance directions");
                if (available)
                    ++feasible;
                else
                    ++boundaries;
            }
        check(feasible == 10 && boundaries == 2, "reference coverage explicitly accounts for ten feasible and two N/A directions");
    }

    void foreign_phase_scheduling()
    {
        const DutPolicy ranked = policy_fixture();
        const DutPolicy legacy = policy_fixture(SelectionMode::Legacy);
        int64_t interval = 0;
        check(interval_us(1, interval) && interval == 2000000, "baseline advertised interval is two seconds");
        check(interval_us(-7, interval) && interval == 7813, "negative intervals round up, never truncate to zero");
        check(!interval_us(-8, interval) && !interval_us(5, interval) && !interval_us(127, interval), "unsupported peer intervals rejected");
        check(candidate_lifetime_us(ranked, 1) == 10000000, "default lifetime max(10s,6s)");
        check(candidate_lifetime_us(ranked, 4) == 48000000, "long interval produces 48 seconds, not ten");
        check(candidate_lifetime_us(legacy, 4) == 10000000, "legacy fixed timeout ignores peer lifetime");
        ForeignSchedule schedule{SelectionMode::Ranked, 10000000, 250000};
        schedule.sent(1000000, 1000100);
        check(schedule.silence_after_us == 0, "original bug: service 250ms after first ranked Announce is permitted");
        schedule.sent(1100000, 1100100, false);
        check(schedule.silence_after_us == 0 && schedule.last_end_us == 1000100, "duplicate neither qualifies nor extends ranked reference");
        schedule.sent(3000000, 3000100);
        check(schedule.silence_after_us == 3250100, "withdrawal deadline begins at second fresh successful transmission");
        check(schedule.no_early_service_until() == 13000000, "no-early bound uses earliest successful TX bound");
        check(schedule.cleanup_until() == 13250100, "cleanup includes delivery/processing uncertainty");
        schedule.sent(4000000, 4000100, false);
        check(schedule.no_early_service_until() == 13000000, "ranked duplicate cannot move expiry");
        schedule.sent(5000000, 5000100);
        check(schedule.no_early_service_until() == 15000000 && schedule.silence_after_us == 3250100,
              "fresh final sequence refreshes expiry without moving withdrawal boundary");
        ForeignSchedule other_source{SelectionMode::Ranked, 10000000, 250000};
        other_source.sent(3000000, 3000100);
        check(other_source.silence_after_us == 0, "one message from another source cannot combine transmitter qualification schedules");
        ForeignSchedule old{SelectionMode::Legacy, 10000000, 250000};
        old.sent(1000000, 1000100);
        check(old.silence_after_us == 1250100, "legacy suppresses after first accepted Announce");
        old.sent(5000000, 5000100, false);
        check(old.no_early_service_until() == 15000000, "intentional legacy duplicate refreshes fixed quiet deadline");
        check(fresh_sequence(65535, 0) && fresh_sequence(0, 32767), "rollover and largest forward delta");
        check(!fresh_sequence(1, 32769) && fresh_sequence(32769, 1) == false, "half-range boundary symmetric rejection");
        check(!fresh_sequence(12, 12) && !fresh_sequence(12, 11) && !fresh_sequence(0, 32768), "duplicates, backwards and half-range rejected");
        AnnounceSequence first{65534};
        AnnounceSequence second{8};
        check(first.next() == 65535 && second.next() == 9 && first.next() == 0, "source-specific counters independent of each other and Delay probes");
    }

    void sequence_policy_fixtures_and_timing()
    {
        const uint16_t expected[][5] = {
            {100, 100, 101, 101, 100},
            {0, 65535, 1, 0, 65534},
            {100, 32868, 101, 32869, 100},
            {65535, 0, 0, 1, 1},
        };
        check(std::size(SequenceScenarios) == 4, "sequence policy has four independently controlled scenarios");
        Config config{};
        config.policy = policy_fixture();
        int8_t logarithm = 0;
        int64_t check_us = 0;
        check(sequence_timing(config, logarithm, check_us) && logarithm == 1 && check_us == 1250000,
              "default policy has a decisive 1.25-second qualification observation and ten-second candidate lease");
        config.policy.foreign_timeout_ms = 1000;
        check(sequence_timing(config, logarithm, check_us) && logarithm == 2 &&
              candidate_lifetime_us(config.policy, logarithm) == 12000000,
              "short ranked timeout selects a longer supported advertised interval for distinguishable expiry");
        config.policy.selection = SelectionMode::Legacy;
        check(!sequence_timing(config, logarithm, check_us),
              "legacy fixed timeout too short for controls is unavailable, not a guessed pass");
        config.policy = policy_fixture();
        config.policy.log_announce_interval = config.policy.log_sync_interval = 4;
        check(sequence_timing(config, logarithm, check_us) && logarithm == 4,
              "slow sixteen-second DUT service gets a forty-eight-second ranked candidate lease");
        config.processing_allowance_ms = config.response_window_ms = config.request_interval_ms = 5000;
        check(!sequence_timing(config, logarithm, check_us),
              "unseparable extreme timing is rejected before any injection");

        for (SelectionMode mode : {SelectionMode::Ranked, SelectionMode::Legacy})
        {
            config = {};
            config.policy = policy_fixture(mode);
            check(sequence_timing(config, logarithm, check_us) && logarithm == 1,
                  "both default selection policies admit the four sequence scenarios");
            for (size_t index = 0; index < std::size(SequenceScenarios); ++index)
            {
                const SequenceScenario &scenario = SequenceScenarios[index];
                check(scenario.first == expected[index][0] && scenario.probe == expected[index][1] &&
                      scenario.qualifier == expected[index][2] && scenario.update == expected[index][3] &&
                      scenario.legacy_update == expected[index][4] &&
                      scenario.rejected == (index != 3),
                      "exact duplicate, backward-zero, half-range and forward-rollover fixtures");
                AnnounceSequence sequence{12345};
                ForeignSchedule schedule{mode, 10000000, 250000};
                check(sequence.record(scenario.first, mode, true) && sequence.value == scenario.first,
                      "first explicit Announce is independent of arbitrary configured seed");
                schedule.sent(1000000, 1000100);
                check(schedule.silence_after_us == (mode == SelectionMode::Legacy ? 1250100 : 0),
                      "first-Announce withdrawal obeys selected policy");
                const bool probe_fresh = sequence.record(scenario.probe, mode, false);
                check(probe_fresh == (index == 3), "only forward rollover is fresh in the second-Announce probes");
                schedule.sent(2500000, 2500100, probe_fresh);
                check(schedule.silence_after_us ==
                      (mode == SelectionMode::Legacy ? 1250100 : index == 3 ? 2750100 : 0),
                      "duplicate/backward/half-range cannot satisfy ranked two-Announce qualification");
                if (mode == SelectionMode::Ranked && scenario.rejected)
                {
                    check(sequence.value == scenario.first, "rejected sequences do not replace the accepted sequence reference");
                    check(sequence.record(scenario.qualifier, mode, false), "valid forward control remains fresh after rejection");
                    schedule.sent(4000000, 4000100);
                    check(schedule.silence_after_us == 4250100, "only accepted forward control qualifies a rejected-probe source");
                }
                const int64_t old_expiry = schedule.no_early_service_until();
                const int64_t update_begin = schedule.last_end_us + schedule.lifetime_us * 2 / 3;
                check(update_begin + schedule.allowance_us < old_expiry, "late update precedes even the earliest original expiry");
                const uint16_t reference = sequence.value;
                const bool update_fresh = sequence.record(
                    mode == SelectionMode::Ranked ? scenario.update : scenario.legacy_update, mode, false);
                check(update_fresh == (index == 3), "late probes remain duplicate/backward/half-range or fresh rollover successor");
                schedule.sent(update_begin, update_begin + 100, update_fresh);
                if (mode == SelectionMode::Ranked && scenario.rejected)
                {
                    check(schedule.no_early_service_until() == old_expiry && sequence.value == reference,
                          "ranked rejected update neither extends expiry nor moves sequence reference");
                    const int64_t deadline = update_begin + schedule.lifetime_us - schedule.allowance_us;
                    check(deadline < update_begin + schedule.lifetime_us &&
                          deadline - old_expiry > 2000000 + 500000 + schedule.allowance_us,
                          "no-refresh proof has time for a fresh Announce and Delay yet precedes any wrongly refreshed expiry");
                }
                else
                {
                    check(schedule.no_early_service_until() == update_begin + 10000000 &&
                          schedule.no_early_service_until() - old_expiry > 6000000,
                          "legacy every-Announce refresh and ranked fresh successor must hold standby beyond old expiry");
                }
                for (AuthenticationMode authentication : {AuthenticationMode::Disabled, AuthenticationMode::Mixed,
                                                           AuthenticationMode::Required})
                {
                    Authenticator auth;
                    check(auth.initialize(1, "0123456789ABCDEFGHIJKLMNOPQRSTUV", 1), "synthetic Announce signing fixture initialized");
                    for (uint16_t wire_sequence : expected[index])
                    {
                        Packet packet{};
                        make_foreign_announce(packet.frame, Requester.data(), Requester.data(), Domain, wire_sequence);
                        Message dataset = parse_valid(golden(11, 0));
                        dataset.priority1 = 127;
                        std::memcpy(dataset.grandmaster, Requester.data(), 8);
                        set_foreign_dataset(packet.frame, dataset, logarithm);
                        size_t length = 64;
                        if (authentication != AuthenticationMode::Disabled)
                        {
                            packet.frame[15] = 0x12;
                            check(auth.append(packet.frame + 14, MaxMessageSize, length), "explicit sequence is included in a valid signature");
                        }
                        packet.length = static_cast<uint16_t>(14 + length);
                        config.domain = Domain;
                        config.policy.authentication = authentication;
                        config.policy.spp = 1;
                        Message decoded{};
                        check(parse_config_message(packet, config, auth, decoded) == ParseResult::Valid &&
                              decoded.type == 11 && decoded.sequence == wire_sequence && decoded.log_interval == logarithm &&
                              std::memcmp(decoded.source, Requester.data(), 10) == 0,
                              "every exact Announce fixture survives unsigned/mixed/required wire encoding with stable source and interval");
                    }
                }
            }
        }
    }

    void interval_policy_fixtures_and_timing()
    {
        constexpr int8_t logarithms[] = {-7, -1, 4, -8, 5, 127, -128};
        constexpr uint8_t encoded[] = {0xf9, 0xff, 0x04, 0xf8, 0x05, 0x7f, 0x80};
        constexpr int64_t ranked_lifetimes[] = {10000000, 10000000, 48000000};
        check(std::size(IntervalScenarios) == std::size(logarithms), "seven independent interval scenarios");
        Config config{};
        config.domain = Domain;
        config.policy = policy_fixture();
        IntervalTiming timing{};
        check(interval_timing(config, timing) && timing.check_us == 1250000 && timing.recovery_us == 6000000,
              "default interval test has exact qualification and bounded full-service recovery budgets");
        config.policy.foreign_timeout_ms = 48000;
        check(!interval_timing(config, timing), "timeout masking every advertised interval cannot claim interval sensitivity");
        config.policy.foreign_timeout_ms = 41500;
        check(!interval_timing(config, timing), "equality at the fixed/long discrimination threshold is unavailable");
        config.policy.foreign_timeout_ms = 41499;
        check(interval_timing(config, timing), "strictly separated fixed/long discrimination threshold is usable");
        config.policy.foreign_timeout_ms = 1000;
        check(!interval_timing(config, timing), "short lifetime cannot expire during the first-Announce qualification check");
        config.policy = policy_fixture();
        config.policy.log_announce_interval = config.policy.log_sync_interval = 4;
        check(!interval_timing(config, timing), "slow service cannot hide an incorrectly refreshed unsupported interval");
        config.policy = policy_fixture();
        config.processing_allowance_ms = config.response_window_ms = config.request_interval_ms = 5000;
        check(!interval_timing(config, timing), "extreme timing cannot generate a vacuous silence-only interval pass");

        for (SelectionMode mode : {SelectionMode::Ranked, SelectionMode::Legacy})
        {
            config = {};
            config.domain = Domain;
            config.policy = policy_fixture(mode);
            check(interval_timing(config, timing), "default ranked and legacy interval matrices both have decisive windows");
            for (size_t index = 0; index < std::size(IntervalScenarios); ++index)
            {
                const IntervalScenario &scenario = IntervalScenarios[index];
                check(scenario.logarithm == logarithms[index] && scenario.supported == (index < 3),
                      "independent exact minimum/negative/long and four unsupported signed-byte fixtures");
                const bool rejected = mode == SelectionMode::Ranked && !scenario.supported;
                const int64_t lifetime = candidate_lifetime_us(config.policy, scenario.supported ? scenario.logarithm : 4);
                check(lifetime == (mode == SelectionMode::Legacy ? 10000000 :
                                   scenario.supported ? ranked_lifetimes[index] : 48000000),
                      "ranked uses advertised/floor lifetime; legacy always uses ten seconds, even for unsupported bytes");
                ForeignSchedule schedule{mode, lifetime, 250000};
                if (rejected)
                {
                    schedule.sent(1000000, 1000100, false);
                    schedule.sent(6000000, 6000100, false);
                    check(schedule.fresh_sends == 0 && schedule.silence_after_us == 0 && schedule.last_end_us == 0,
                          "unsupported interval cannot contribute qualification or move ranked expiry");
                }
                schedule.sent(11000000, 11000100);
                if (mode == SelectionMode::Ranked)
                {
                    check(schedule.silence_after_us == 0, "first supported interval must preserve healthy ranked service");
                    schedule.sent(12250000, 12250100);
                }
                check(schedule.silence_after_us != 0 &&
                      schedule.no_early_service_until() - schedule.last_begin_us == lifetime,
                      "qualified interval has a nonvacuous silence boundary and exact earliest expiry");
                const int64_t original_expiry = schedule.no_early_service_until();
                const int64_t recovery_deadline = schedule.cleanup_until() + timing.recovery_us;
                if (rejected)
                {
                    const int64_t update_begin = schedule.last_end_us + lifetime * 2 / 3;
                    schedule.sent(update_begin, update_begin + 100, false);
                    check(schedule.no_early_service_until() == original_expiry &&
                          recovery_deadline < update_begin + lifetime - schedule.allowance_us,
                          "fresh unsupported update cannot shorten/extend expiry; recovery proves no refresh before a wrongly renewed lease");
                }
                else if (mode == SelectionMode::Legacy || scenario.logarithm < 0)
                    check(recovery_deadline < schedule.last_begin_us + 48000000,
                          "fixed/negative recovery must precede an incorrectly applied long-interval lease");
                else
                    check(original_expiry > schedule.last_end_us + 10000000 + timing.recovery_us,
                          "long ranked standby must continue beyond fixed-timeout recovery, not just eventually resume");

                for (AuthenticationMode authentication : {AuthenticationMode::Disabled, AuthenticationMode::Mixed,
                                                           AuthenticationMode::Required})
                {
                    Authenticator auth;
                    check(auth.initialize(1, "0123456789ABCDEFGHIJKLMNOPQRSTUV", 1), "synthetic interval signing fixture initialized");
                    Packet packet{};
                    make_foreign_announce(packet.frame, Requester.data(), Requester.data(), Domain, 104);
                    Message dataset = parse_valid(golden(11, 0));
                    dataset.priority1 = 127;
                    std::memcpy(dataset.grandmaster, Requester.data(), 8);
                    set_foreign_dataset(packet.frame, dataset, scenario.logarithm);
                    check(packet.frame[47] == encoded[index], "advertised interval has independent exact signed-byte encoding");
                    size_t length = 64;
                    if (authentication != AuthenticationMode::Disabled)
                    {
                        packet.frame[15] = 0x12;
                        check(auth.append(packet.frame + 14, MaxMessageSize, length) &&
                              auth.verify(packet.frame + 14, length) == AuthResult::Valid,
                              "unsupported interval remains a correctly signed PTP message, not an authentication rejection");
                    }
                    packet.length = static_cast<uint16_t>(14 + length);
                    config.policy.authentication = authentication;
                    config.policy.spp = 1;
                    Message decoded{};
                    check(parse_config_message(packet, config, auth, decoded) == ParseResult::Valid &&
                          decoded.log_interval == logarithms[index] && decoded.sequence == 104 &&
                          std::memcmp(decoded.source, Requester.data(), 10) == 0,
                          "every interval fixture reaches selection after unsigned/mixed/required parsing");
                }
            }
        }
        int64_t value = 0;
        for (int logarithm = -128; logarithm <= 127; ++logarithm)
        {
            const bool supported = logarithm >= -7 && logarithm <= 4;
            check(interval_us(static_cast<int8_t>(logarithm), value) == supported,
                  "all possible signed interval bytes have the exact supported bounds without shift overflow");
        }
    }

    void policy_classification_and_ordering()
    {
        check(compatible_with_firmware_mode(Test::Announce, FirmwareMode::Unknown) &&
                  compatible_with_firmware_mode(Test::Announce, FirmwareMode::PtpOnly) &&
                  compatible_with_firmware_mode(Test::Announce, FirmwareMode::Combined) &&
                  !compatible_with_firmware_mode(Test::Announce, FirmwareMode::NtpOnly),
              "ordinary PTP tests are unavailable only to NTP-only firmware");
        check(compatible_with_firmware_mode(Test::PtpOnly, FirmwareMode::PtpOnly) &&
                  !compatible_with_firmware_mode(Test::PtpOnly, FirmwareMode::Unknown) &&
                  !compatible_with_firmware_mode(Test::PtpOnly, FirmwareMode::NtpOnly) &&
                  !compatible_with_firmware_mode(Test::PtpOnly, FirmwareMode::Combined),
              "PTP-only test is available only to PTP-only firmware");
        check(compatible_with_firmware_mode(Test::CombinedLoad, FirmwareMode::Combined) &&
                  !compatible_with_firmware_mode(Test::CombinedLoad, FirmwareMode::Unknown) &&
                  !compatible_with_firmware_mode(Test::CombinedLoad, FirmwareMode::NtpOnly) &&
                  !compatible_with_firmware_mode(Test::CombinedLoad, FirmwareMode::PtpOnly),
              "NTP-dependent PTP scenario is available only to combined firmware");
        Config target{};
        std::copy(Master.begin(), Master.begin() + 6, target.target_mac);
        target.domain = Domain;
        CleanupLease lease{};
        lease.attempted(target, 1000100, 10000000, 250000);
        check(lease.pending && lease.deadline_us == 11250100, "even an uncertain/failed TX reserves possible candidate lifetime for cleanup");
        lease.attempted(target, 2000100, 10000000, 250000);
        check(lease.deadline_us == 12250100, "cleanup follows last possibly delivered transmission");
        check(lease.matches(target), "cleanup tied to exact selected target MAC and domain");
        Config other = target;
        ++other.domain;
        check(!lease.matches(other), "different domain cannot certify previous target cleanup");
        other = target;
        other.target_mac[5] ^= 2;
        check(!lease.matches(other), "different MAC cannot certify previous target cleanup");
        lease.completed(false);
        check(lease.pending && lease.deadline_us == 12250100, "unhealthy cleanup retains blocker and expiry bound");
        lease.completed(true);
        check(!lease.pending && lease.deadline_us == 0, "only recovered healthy controls clear cleanup gate");
        RotationRestoreLease rotation{};
        target.key_id = 17;
        rotation.begin(target);
        check(rotation.pending && rotation.matches(target), "rotation cleanup is tied to the target, domain and original key ID");
        other = target;
        ++other.key_id;
        check(!rotation.matches(other), "a different key cannot certify interrupted fixture restoration");
        other = target;
        ++other.domain;
        check(!rotation.matches(other), "a different target domain cannot clear the rotation cleanup gate");
        rotation.completed(false);
        check(rotation.pending, "unverified rotation cleanup keeps dependent tests blocked");
        rotation.completed(true);
        check(!rotation.pending, "only verified restoration clears the rotation cleanup gate");
        check(classify_foreign(Result::Pass, true, false, false) == Result::Pass, "healthy full policy run passes");
        check(classify_foreign(Result::Fail, true, false, false) == Result::Fail, "valid baseline plus policy contradiction fails");
        check(classify_foreign(Result::CouldNotRun, true, false, false) == Result::CouldNotRun, "missing TX/healthy controls cannot become pass");
        for (Result observed : {Result::Pass, Result::Fail, Result::CouldNotRun})
        {
            check(classify_foreign(observed, false, false, false) == Result::CouldNotRun, "cleanup failure invalidates silence-only result");
            check(classify_foreign(observed, true, true, false) == Result::CouldNotRun, "link/timestamp/RX interruption takes precedence");
            check(classify_foreign(observed, true, false, true) == Result::CouldNotRun, "cancellation is never success");
        }
        std::array<bool, static_cast<size_t>(Test::Count)> seen{};
        const std::array<Test, static_cast<size_t>(Test::Count)> expected_order = {{
            Test::CombinedLoad,
            Test::Announce, Test::SyncFollowUp, Test::DelayExchange, Test::TimeProperties,
            Test::Measurement, Test::Sustained, Test::NegativeDelay, Test::ForeignAnnounce, Test::PtpOnly,
            Test::RankingPrecedence, Test::SourceIsolation, Test::SequencePolicy, Test::DatasetChanges,
            Test::MultipleMasters, Test::IntervalExpiry,
            Test::SignedMessages, Test::SignedExchange, Test::InvalidAuthentication,
            Test::AuthenticationPolicy, Test::AuthenticationBeforeSelection,
            Test::OptionalUnsignedService, Test::OptionalSignedRejection,
            Test::AnnounceAuthenticationPolicy, Test::InvalidAuthenticationRefresh, Test::ForeignCapacity,
            Test::SelectionNtpLoad, Test::SelectionNtpPolicyLoad,
            Test::GnssRecovery, Test::PpsRecovery, Test::LinkRecovery, Test::RestartRecovery,
            Test::StandbyRecovery,
            Test::StandbyPpsRecovery, Test::StandbyLinkRecovery, Test::StandbyRestartRecovery,
            Test::KeyRotation, Test::StartupListening, Test::StandbyExpiryUnqualified,
            Test::SelectionNtpHoldover,
        }};
        bool assisted = false;
        size_t combined_position = seen.size();
        size_t gnss_position = seen.size();
        std::array<size_t, 4> group_counts{};
        unsigned previous_group = 0;
        for (size_t position = 0; position < seen.size(); ++position)
        {
            const size_t index = ordered_test_index(position);
            check(index < seen.size() && !seen[index], "ordering contains every stable test ID exactly once");
            seen[index] = true;
            const Test test = static_cast<Test>(index);
            check(test == expected_order[position], "mixed NTP/PTP precedes PTP within unattended and assisted groups");
            check(!assisted || requires_operator(test),
                  "all unattended PTP tests precede every assisted PTP test");
            assisted |= requires_operator(test);
            const unsigned group = (requires_operator(test) ? 2U : 0U) + (involves_ntp(test) ? 0U : 1U);
            if (position < 23)
            {
                check(group >= previous_group, "original unattended NTP/PTP subgroup ordering remains stable");
                previous_group = group;
            }
            ++group_counts[group];
            if (position < 28)
            {
                if (requires_operator(test))
                    assisted = true;
                else
                    check(!assisted, "original unattended scenarios retain their positions before assisted scenarios");
            }
            if (test == Test::CombinedLoad) combined_position = position;
            if (test == Test::GnssRecovery) gnss_position = position;
            if (test == Test::StandbyRecovery)
                check(pending_reason(test) == nullptr && requires_operator(test) && requires_suppression_opt_in(test),
                      "standby recovery is implemented and requires both operator assistance and foreign-master opt-in");
            if (test == Test::DatasetChanges || test == Test::MultipleMasters)
                check(pending_reason(test) == nullptr && !requires_operator(test) && requires_suppression_opt_in(test),
                      "dataset/reflection and overlapping masters are implemented, unattended and opt-in gated");
            if (test == Test::RankingPrecedence)
                check(pending_reason(test) == nullptr, "ranking precedence is implemented with boundary-aware device scenarios");
            if (test == Test::SourceIsolation)
                check(pending_reason(test) == nullptr, "source isolation is implemented as a two-source ranked qualification scenario");
            if (test == Test::SequencePolicy)
                check(pending_reason(test) == nullptr && !requires_operator(test),
                      "sequence policy is implemented as an unattended device scenario");
            if (test == Test::IntervalExpiry)
                check(pending_reason(test) == nullptr && !requires_operator(test) && requires_suppression_opt_in(test),
                      "interval expiry is implemented as an unattended, authorized foreign-master device scenario");
            if (test >= Test::SignedMessages && test <= Test::AuthenticationBeforeSelection)
                check(pending_reason(test) == nullptr, "implemented authentication scenarios no longer pending");
        }
        check(combined_position + 21 == 21, "unattended combined load is display test 21 after twenty NTP/IP tests");
        check(gnss_position + 21 == 49, "all assisted PTP tests follow unattended PTP tests");
        check(group_counts == std::array<size_t, 4>{{3, 25, 1, 11}},
              "appended scenarios preserve their unattended, combined and assisted classifications");
        check(seen.size() + 20 == 60, "catalog contains 48 original and twelve appended display tests");
        check(static_cast<Test>(ordered_test_index(21)) == Test::OptionalUnsignedService &&
              static_cast<Test>(ordered_test_index(22)) == Test::OptionalSignedRejection &&
              !requires_operator(Test::OptionalUnsignedService) && !requires_operator(Test::OptionalSignedRejection) &&
              requires_suppression_opt_in(Test::OptionalSignedRejection),
              "new unattended rows 42/43 are immediately before the first assisted PTP test");
        check(static_cast<Test>(ordered_test_index(27)) == Test::SelectionNtpPolicyLoad &&
              static_cast<Test>(ordered_test_index(28)) == Test::GnssRecovery &&
              static_cast<Test>(ordered_test_index(32)) == Test::StandbyRecovery &&
              static_cast<Test>(ordered_test_index(seen.size() - 1)) == Test::SelectionNtpHoldover,
              "unattended appended tests precede original assisted tests without changing stable engine IDs");
        check(requires_operator(Test::StandbyRecovery) && !involves_ntp(Test::StandbyRecovery) &&
                  compatible_with_firmware_mode(Test::StandbyRecovery, FirmwareMode::PtpOnly),
              "standby fault/restoration remains assisted and does not require NTP");
        check(ordered_test_index(seen.size()) == seen.size(), "invalid order position returns explicit sentinel");
    }
}

namespace
{
    void optional_authentication_policy()
    {
        Config config{};
        config.policy = policy_fixture();
        config.policy.authentication = AuthenticationMode::Mixed;
        config.policy.spp = 1;
        config.domain = Domain;
        Authenticator missing;
        Authenticator signer;
        check(signer.initialize(1, "0123456789ABCDEFGHIJKLMNOPQRSTUV", 1), "public synthetic signer initialized");
        check(!missing.available(), "absent tester key does not initialize crypto");
        for (Test test : {Test::Announce, Test::SyncFollowUp, Test::DelayExchange, Test::Sustained,
                          Test::ForeignAnnounce, Test::GnssRecovery, Test::LinkRecovery,
                          Test::OptionalUnsignedService, Test::OptionalSignedRejection})
            check(policy_prerequisite(test, config) == nullptr, "optional keyless policy can run existing and new tests");
        config.key_id = 1;
        check(policy_prerequisite(Test::Announce, config) != nullptr, "id-only optional key is not silently treated as missing");
        config.key_id = 0;
        config.key_value = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
        check(policy_prerequisite(Test::Announce, config) != nullptr, "value-only optional key is not silently treated as missing");
        config.key_value = "";
        check(policy_prerequisite(Test::Announce, config) == nullptr, "empty and absent values both permit optional service");
        for (uint8_t type : {uint8_t{0}, uint8_t{8}, uint8_t{9}, uint8_t{11}})
        {
            Packet packet{};
            auto bytes = golden(type, 0);
            if (type == 11)
            {
                put(bytes, 6, PtpTimescaleFlag | UtcValidFlag, 2);
                bytes[48] = 248;
                bytes[49] = 0xfe;
                put(bytes, 50, 65535, 2);
                put(bytes, 61, 0, 2);
            }
            const size_t body = bytes.size();
            std::copy(bytes.begin(), bytes.end(), packet.frame + 14);
            packet.length = static_cast<uint16_t>(std::max<size_t>(60, 14 + body));
            Message message{};
            check(parse_config_message(packet, config, missing, message) == ParseResult::Valid &&
                  !message.authenticated && message.minor_version == 0,
                  "all optional keyless outgoing types parse as unsigned v2.0 without a key");
            message.log_interval = type == 11 ? 1 : type == 9 ? 127 : 0;
            check(wire_policy_error(config, message) == nullptr, "mixed source policy permits effective unsigned service");
            check(restricted_output(packet, message), "base lengths, legacy controls and unsigned restricted wire format");
            if (type == 11)
                check(supported_properties(message), "optional unsigned Announce preserves qualified GNSS/UTC properties");
            ServiceAuthentication mode;
            Message announce = message;
            announce.type = 11;
            check(mode.observe(announce) == nullptr && mode.learned && !mode.authenticated &&
                  mode.observe(message) == nullptr, "Announce latches unsigned mode for all service types");
            config.policy.authentication = AuthenticationMode::Required;
            check(parse_config_message(packet, config, missing, message) == ParseResult::AuthenticationInvalid,
                  "required authentication never permits keyless unsigned output");
            config.policy.authentication = AuthenticationMode::Mixed;
            bytes.resize(MaxMessageSize);
            size_t length = body;
            promote_to_v21(bytes.data());
            check(signer.append(bytes.data(), bytes.size(), length), "matching valid signed service fixture");
            std::copy_n(bytes.begin(), length, packet.frame + 14);
            packet.length = static_cast<uint16_t>(14 + length);
            check(parse_config_message(packet, config, missing, message) != ParseResult::Valid,
                  "valid signed server remains unverifiable without a tester key, never reclassified unsigned");
            check(parse_config_message(packet, config, signer, message) == ParseResult::Valid && message.authenticated,
                  "optional valid matching keys preserve signed service");
            message.log_interval = type == 11 ? 1 : type == 9 ? 127 : 0;
            check(wire_policy_error(config, message) == nullptr && mode.observe(message) != nullptr,
                  "individually valid signed packet still cannot join an unsigned broadcast stream");
            config.policy.authentication = AuthenticationMode::Disabled;
            check(parse_config_message(packet, config, signer, message) == ParseResult::AuthenticationInvalid,
                  "disabled policy rejects signed traffic even if the caller has a valid key");
            config.policy.authentication = AuthenticationMode::Mixed;
            check(parse_config_message(packet, config, signer, message) == ParseResult::Valid,
                  "policy rejection does not mutate or strip the signed packet");
            ServiceAuthentication signed_mode;
            announce = message;
            announce.type = 11;
            check(signed_mode.observe(announce) == nullptr && signed_mode.authenticated, "verified signed Announce latches signed mode");
            message.authenticated = false;
            message.minor_version = 0;
            check(signed_mode.observe(message) != nullptr, "unsigned traffic cannot downgrade an established signed master");
            packet.frame[packet.length - 1] ^= 1;
            check(parse_config_message(packet, config, signer, message) == ParseResult::AuthenticationInvalid,
                  "verification failure is explicit; never an unsigned fallback");
        }
        Message unsigned_master = parse_valid(golden(11, 0));
        Message signed_master = unsigned_master;
        signed_master.authenticated = true;
        signed_master.minor_version = 1;
        for (Test test : {Test::SignedMessages, Test::SignedExchange, Test::InvalidAuthentication,
                          Test::AuthenticationPolicy, Test::AuthenticationBeforeSelection})
            check(service_prerequisite(test, unsigned_master) != nullptr &&
                  service_prerequisite(test, signed_master) == nullptr,
                  "existing signed tests are explicitly inapplicable, never false passes for keyless service");
        for (Test test : {Test::OptionalUnsignedService, Test::OptionalSignedRejection})
        {
            check(service_prerequisite(test, unsigned_master) == nullptr &&
                  service_prerequisite(test, signed_master) != nullptr, "new rows only run against effective unsigned service");
            for (AuthenticationMode policy : {AuthenticationMode::Disabled, AuthenticationMode::Required, AuthenticationMode::Unknown})
            {
                config.policy.authentication = policy;
                check(policy_prerequisite(test, config) != nullptr, "new rows never infer optional policy from unsigned packets alone");
            }
        }
    }

    void optional_signed_probe_fixtures()
    {
        Authenticator signer;
        Authenticator missing;
        check(signer.initialize(1, "0123456789ABCDEFGHIJKLMNOPQRSTUV", 1), "public rejection fixture initialized");
        const uint8_t mac[] = {2, 3, 4, 5, 6, 7};
        for (bool announce : {false, true})
            for (AuthMutation mutation : KeylessProbes)
            {
                Packet packet{};
                if (announce)
                {
                    make_foreign_announce(packet.frame, mac, Requester.data(), Domain, 65535);
                    Message dataset = parse_valid(golden(11, 0));
                    dataset.priority1 = 127;
                    std::copy(Requester.begin(), Requester.begin() + 8, dataset.grandmaster);
                    set_foreign_dataset(packet.frame, dataset, 4);
                }
                else
                    make_delay_request(packet.frame, mac, Requester.data(), Domain, 0, 65535);
                const size_t body = announce ? 64 : 44;
                size_t length = body;
                check(sign_rejection_probe(packet.frame + 14, MaxMessageSize, length, signer, mutation),
                      "production rejection probe builder produces bounded signed-looking input");
                check(packet.frame[15] == 0x12 && packet.frame[46] == 0 &&
                      read16(packet.frame + 44) == 65535 &&
                      std::equal(Requester.begin(), Requester.end(), packet.frame + 34),
                      "probe has exact v2.1 header, source, control and sequence, never a framing-only rejection");
                check(length == body + (mutation == AuthMutation::Duplicate ? 52 :
                                        mutation == AuthMutation::Truncated ? 24 : 26), "exact signed/mutated probe lengths");
                if (mutation == AuthMutation::Count)
                    check(signer.verify(packet.frame + 14, length) == AuthResult::Valid,
                          "unmutated signed fixture is a real valid HMAC, not merely a fake TLV");
                else
                    check(signer.verify(packet.frame + 14, length) != AuthResult::Valid,
                          "each declared tampering mutation independently fails verification");
                Message parsed{};
                packet.length = static_cast<uint16_t>(14 + length);
                Config config{};
                config.domain = Domain;
                config.policy = policy_fixture();
                config.policy.authentication = AuthenticationMode::Mixed;
                check(parse_config_message(packet, config, missing, parsed) != ParseResult::Valid &&
                      !parsed.authenticated, "no signed-looking input becomes an accepted unsigned packet without crypto");
            }
        uint8_t message[44 + AuthTlvSize] = {};
        size_t length = 44;
        check(!sign_rejection_probe(message, sizeof(message), length, signer, AuthMutation::Duplicate) && length == 44,
              "duplicate-TLV probe cannot overrun signing capacity");
    }

    void authenticated_wire_vectors()
    {
        constexpr char Key[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";
        constexpr uint8_t ExpectedIcv[] = {
            0xf9, 0xb7, 0x40, 0xac, 0x78, 0x3d, 0xd5, 0x40,
            0x19, 0x09, 0x3b, 0xbb, 0xe1, 0x37, 0x06, 0xe2
        };
        Authenticator auth;
        check(auth.initialize(0x1234, Key, 7), "PTP key initialized independently");
        for (uint8_t type : {uint8_t{0}, uint8_t{1}, uint8_t{8}, uint8_t{9}, uint8_t{11}})
        {
            auto bytes = golden(type);
            const size_t body = bytes.size();
            bytes.resize(MaxMessageSize);
            size_t length = body;
            check(auth.append(bytes.data(), bytes.size(), length), "all server message types signed");
            check(length == body + 26 && read16(bytes.data() + 2) == length, "exact signed messageLength");
            check(bytes[body] == 0x80 && bytes[body + 1] == 9 && bytes[body + 3] == 22 &&
                  bytes[body + 4] == 7 && bytes[body + 5] == 0, "exact immediate authentication TLV");
            check(auth.verify(bytes.data(), length) == AuthResult::Valid, "signed wire image verified");
            Message message{};
            check(parse_message(bytes.data(), length, Domain, message, &auth) == ParseResult::Valid &&
                  message.authenticated, "parser admits independently verified signed message");
            if (type == 1)
                check(std::equal(std::begin(ExpectedIcv), std::end(ExpectedIcv), bytes.begin() + static_cast<ptrdiff_t>(body + 10)),
                      "matches independent .NET HMAC-SHA256-128 golden ICV, not just a round trip");
            for (size_t truncated = body + 1; truncated < length; ++truncated)
                check(auth.verify(bytes.data(), truncated) != AuthResult::Valid, "every authentication truncation rejected");
            for (size_t offset = 0; offset < length; ++offset)
            {
                bytes[offset] ^= 1;
                check(auth.verify(bytes.data(), length) != AuthResult::Valid, "every signed byte covered by structure/ICV validation");
                bytes[offset] ^= 1;
            }
            Authenticator other_key;
            check(other_key.initialize(0x1234, "1123456789ABCDEFGHIJKLMNOPQRSTUV", 7), "independent wrong key fixture");
            check(other_key.verify(bytes.data(), length) == AuthResult::InvalidMac, "wrong key value cannot verify");
            for (size_t index = 0; index < static_cast<size_t>(AuthMutation::Count); ++index)
            {
                auto mutated = bytes;
                size_t changed_length = length;
                const auto mutation = static_cast<AuthMutation>(index);
                if (mutation == AuthMutation::WrongKey)
                {
                    mutated = golden(type);
                    changed_length = mutated.size();
                    mutated.resize(MaxMessageSize);
                    check(other_key.append(mutated.data(), mutated.size(), changed_length), "wrong-value request signed with actual alternate key");
                }
                changed_length = mutate_authentication(mutated.data(), changed_length, mutation);
                check(auth.verify(mutated.data(), changed_length) != AuthResult::Valid,
                      std::string("negative authentication mutation rejected: ") + AuthMutationNames[index]);
            }
            Config config{};
            config.policy = policy_fixture();
            config.policy.authentication = AuthenticationMode::Required;
            config.policy.spp = 7;
            config.domain = Domain;
            Packet packet{};
            packet.length = static_cast<uint16_t>(14 + length);
            std::copy_n(bytes.begin(), length, packet.frame + 14);
            check(parse_config_message(packet, config, auth, message) == ParseResult::Valid, "required service accepts verified ICV");
            auto unsigned_bytes = golden(type);
            packet.length = static_cast<uint16_t>(14 + unsigned_bytes.size());
            std::copy(unsigned_bytes.begin(), unsigned_bytes.end(), packet.frame + 14);
            check(parse_config_message(packet, config, auth, message) == ParseResult::AuthenticationInvalid,
                  "DUT output under signed policy cannot silently downgrade to unsigned");
            config.policy.authentication = AuthenticationMode::Disabled;
            Authenticator unsigned_auth;
            packet.length = static_cast<uint16_t>(14 + length);
            std::copy_n(bytes.begin(), length, packet.frame + 14);
            check(parse_config_message(packet, config, unsigned_auth, message) != ParseResult::Valid,
                  "authenticated service under an unsigned policy remains an explicit policy mismatch");
        }
        Authenticator missing;
        check(!missing.initialize(0, Key, 7), "zero key ID rejected");
        check(!missing.initialize(1, "short", 7), "short key rejected");
        check(!missing.initialize(1, "0123456789ABCDEFGHIJKLMNOPQRSTU!", 7), "non-alphanumeric key rejected");
        auto bytes = golden(1);
        size_t length = bytes.size();
        bytes.resize(length + AuthTlvSize - 1);
        check(!auth.append(bytes.data(), bytes.size(), length), "signing capacity checked");
        Config config{};
        config.policy = policy_fixture();
        check(policy_prerequisite(Test::SignedMessages, config) != nullptr, "unsigned DUT cannot pass signed scenario");
        check(!auth.initialize(1, Key, 7), "live key cannot be overwritten/leaked");
        check(classify_authenticated_probe(true, true, false, false, false, false, 0, false) == Result::Pass,
              "required/invalid authentication rejection passes only with both signed controls");
        check(classify_authenticated_probe(true, true, true, true, true, false, 0, false) == Result::Pass,
              "mixed unsigned acceptance requires verified signed response");
        check(classify_authenticated_probe(true, true, true, false, true, false, 0, false) == Result::Fail,
              "mixed policy cannot pass on an unverified response");
        check(classify_authenticated_probe(true, true, true, true, false, false, 0, false) == Result::Fail,
              "a matching response to an unauthorized request fails");
        check(classify_authenticated_probe(true, true, false, false, true, false, 0, false) == Result::Fail,
              "mixed policy cannot pass unsigned-request silence");
        check(classify_authenticated_probe(true, true, false, false, false, false, 0, true) == Result::Fail,
              "unrelated malformed DUT output cannot be hidden by a rejection pass");
        for (unsigned missing_control = 0; missing_control < 2; ++missing_control)
            check(classify_authenticated_probe(missing_control != 0, missing_control == 0, false, false, false, false, 0, false) == Result::CouldNotRun,
                  "silence with either signed control unavailable is inconclusive");
        check(classify_authenticated_probe(true, true, false, false, false, true, 0, true) == Result::CouldNotRun,
              "cancelled negative observations cannot pass or attribute a DUT failure");
        check(classify_authenticated_probe(true, true, false, false, false, false, 1, true) == Result::CouldNotRun,
              "RX overflow takes precedence over apparent rejection/failure");
    }
}

namespace
{
    void combined_load_schedule()
    {
        constexpr int64_t Start = 601897829;
        constexpr int64_t Interval = 100000;
        constexpr int64_t End = Start + 20500000;
        constexpr int64_t CollectionEnd = 622397843;
        check(ntp_load::missed_slots(End, CollectionEnd, End, Interval) == 0,
              "reported 14-us collection overrun cannot create a 206th slot after 205 successful requests");
        check(ntp_load::missed_slots(End, End + 250000, End, Interval) == 0,
              "entire scheduling allowance adds no endpoint slots");
        check(ntp_load::missed_slots(End - Interval, CollectionEnd, End, Interval) == 1,
              "a genuine missed final request remains inconclusive despite collection overrun");
        check(ntp_load::missed_slots(End - 2 * Interval, CollectionEnd, End, Interval) == 2,
              "multiple genuine misses are counted without a phantom endpoint slot");
        check(ntp_load::missed_slots(Start, Start, End, Interval) == 0,
              "a slot exactly due can still be sent");
        check(ntp_load::missed_slots(Start, Start + 1, End, Interval) == 1,
              "a pending slot passed while a previous request was outstanding is missed");
        check(ntp_load::missed_slots(Start + Interval, Start, End, Interval) == 0,
              "early response cannot count a future slot as missed");
        for (int64_t rate : {int64_t{1}, int64_t{10}, int64_t{100}})
        {
            const int64_t interval = 1000000 / rate;
            int64_t next = Start;
            uint64_t misses = 0;
            size_t attempts = 0;
            while (next < End)
            {
                ++attempts;
                const int64_t finished = next + 1000;
                next += interval;
                const uint64_t missed = ntp_load::missed_slots(next, finished, End, interval);
                misses += missed;
                next += static_cast<int64_t>(missed) * interval;
            }
            misses += ntp_load::missed_slots(next, CollectionEnd, End, interval);
            check(attempts == static_cast<size_t>((End - Start + interval - 1) / interval) && misses == 0,
                  "on-time requests at supported rate boundaries complete with no artificial misses");
        }
    }

    void ntp_load_response_validation()
    {
            std::array<uint8_t, 72> packet{};
            packet[0] = static_cast<uint8_t>((4U << 3) | 4U);
            packet[1] = 1;
            const uint64_t request = 0x123456789abcdef0ULL;
            const auto write_timestamp = [&](size_t offset, uint64_t value)
            {
                for (size_t index = 0; index < 8; ++index)
                    packet[offset + 7 - index] = static_cast<uint8_t>(value >> (index * 8));
            };
            write_timestamp(24, request);
            write_timestamp(32, 1);
            write_timestamp(40, 2);
            check(ntp_load::valid_response(packet.data(), 48, 4, request, false),
                  "unsigned load accepts a structurally valid matching NTPv4 response");
            check(ntp_load::valid_response(packet.data(), 72, 4, request, true),
                  "authenticated load accepts the exact signed response length before MAC verification");
            check(!ntp_load::valid_response(nullptr, 72, 4, request, true) &&
                  !ntp_load::valid_response(packet.data(), 47, 4, request, false) &&
                  !ntp_load::valid_response(packet.data(), 48, 4, request, true) &&
                  !ntp_load::valid_response(packet.data(), 71, 4, request, true),
                  "missing, truncated and unsigned responses cannot satisfy signed load");
            check(!ntp_load::valid_response(packet.data(), 72, 3, request, true) &&
                  !ntp_load::valid_response(packet.data(), 72, 4, request + 1, true),
                  "wrong NTP version and mismatched originate timestamps are rejected");
            for (uint8_t invalid : {uint8_t{0}, uint8_t{16}})
            {
                packet[1] = invalid;
                check(!ntp_load::valid_response(packet.data(), 72, 4, request, true),
                      "invalid or kiss-of-death stratum is not a successful load response");
            }
            packet[1] = 1;
            packet[0] |= 0xc0;
            check(!ntp_load::valid_response(packet.data(), 72, 4, request, true),
                  "unsynchronized NTP server cannot pass load");
            packet[0] = static_cast<uint8_t>((4U << 3) | 3U);
            check(!ntp_load::valid_response(packet.data(), 72, 4, request, true),
                  "client-mode packet cannot pass as a server reply");
            packet[0] = static_cast<uint8_t>((4U << 3) | 4U);
            write_timestamp(32, 0);
            check(!ntp_load::valid_response(packet.data(), 72, 4, request, true),
                  "missing receive timestamp is invalid");
            write_timestamp(32, 1);
            write_timestamp(40, 0);
            check(!ntp_load::valid_response(packet.data(), 72, 4, request, true),
                  "missing transmit timestamp is invalid");
            write_timestamp(40, 2);
            using ntp_load::AuthRejectionResponse;
            check(ntp_load::classify_auth_rejection(packet.data(), 48, 4, request) == AuthRejectionResponse::Accepted,
                  "a usable matching response to a negative probe is classified as acceptance");
            check(ntp_load::classify_auth_rejection(packet.data(), 52, 4, request) == AuthRejectionResponse::Rejected,
                  "a matching zero-key rejection trailer is recognized");
            packet[1] = 0;
            for (const char *code : {"AUTH", "CRYP", "NKEY", "DENY", "RSTR"})
            {
                std::memcpy(packet.data() + 12, code, 4);
                check(ntp_load::classify_auth_rejection(packet.data(), 48, 4, request) == AuthRejectionResponse::Rejected,
                      "a matching authentication-related kiss-of-death is recognized");
            }
            std::memcpy(packet.data() + 12, "RATE", 4);
            check(ntp_load::classify_auth_rejection(packet.data(), 48, 4, request) == AuthRejectionResponse::Invalid &&
                  ntp_load::classify_auth_rejection(packet.data(), 48, 3, request) == AuthRejectionResponse::Invalid &&
                  ntp_load::classify_auth_rejection(packet.data(), 48, 4, request + 1) == AuthRejectionResponse::Invalid &&
                  ntp_load::classify_auth_rejection(nullptr, 48, 4, request) == AuthRejectionResponse::Invalid,
                  "unrelated KoDs, wrong versions, unrelated requests and missing packets are inconclusive");
            ntp_load::PolicyProbeEvidence evidence{};
            evidence.start_us = 1000;
            evidence.end_us = 2000;
            for (size_t slot = 0; slot < 2; ++slot)
            {
                evidence.before[slot] = evidence.after[slot] = true;
                evidence.invalid_mac[slot] = ntp_load::ProbeResult::Pass;
                evidence.unsigned_required[slot] = ntp_load::ProbeResult::Pass;
            }
            using ntp_load::ProbeResult;
            check(ntp_load::classify_policy_probes(evidence, true, 1000, 2000, true) == ProbeResult::Pass,
                  "both families pass only when all required-policy probes and controls fit inside the load window");
            evidence.unsigned_required[1] = ProbeResult::Unavailable;
            check(ntp_load::classify_policy_probes(evidence, true, 900, 2100, true) == ProbeResult::Unavailable &&
                  ntp_load::classify_policy_probes(evidence, false, 900, 2100, true) == ProbeResult::Pass,
                  "missing required unsigned rejection is inconclusive but is not applicable to mixed policy");
            evidence.invalid_mac[0] = ProbeResult::Fail;
            check(ntp_load::classify_policy_probes(evidence, false, 900, 2100, true) == ProbeResult::Fail,
                  "a completed negative probe that receives service fails");
            evidence.after[0] = false;
            check(ntp_load::classify_policy_probes(evidence, false, 900, 2100, true) == ProbeResult::Unavailable,
                  "an absent signed post-control overrides a negative-probe failure");
            evidence.after[0] = true;
            evidence.invalid_mac[0] = ProbeResult::Pass;
            check(ntp_load::classify_policy_probes(evidence, false, 1001, 2100, true) == ProbeResult::Unavailable &&
                  ntp_load::classify_policy_probes(evidence, false, 900, 1999, true) == ProbeResult::Unavailable &&
                  ntp_load::classify_policy_probes(evidence, false, 900, 2100, false) == ProbeResult::Unavailable,
                  "outside-window probes or a tester link change are inconclusive");
    }
}

int main()
{
    struct TestCase
    {
        const char *name;
        void (*run)();
    };
    const TestCase cases[] = {
        {"combined NTP load half-open scheduling and genuine missed slots", combined_load_schedule},
        {"signed and unsigned NTP load response structure", ntp_load_response_validation},
        {"golden messages", golden_messages},
        {"headers and truncation", header_and_truncation},
        {"version-aware control fields and authenticated integrity", versioned_control_fields},
        {"v2.1 header promotion and deliberate probe preservation", v21_header_promotion},
        {"identities, timestamps and Announce", identities_timestamps_and_announce},
        {"TLV boundaries and authentication fail-closed", tlv_boundaries_and_authentication},
        {"signed corrections including INT64_MIN", signed_corrections},
        {"hardware Sync RX with untimestamped PTP general messages", receive_timestamp_policy},
        {"pairing, expiration and sequence wrap", pairing},
        {"delay response matching and request wire image", delay_exchange},
        {"measurements and correction arithmetic", measurements},
        {"all negative request builders: exact bytes, boundaries and tester policy", negative_request_builders},
        {"negative classification: before/after probes and interruption/drop precedence", negative_classification},
        {"authenticated negative request builders: signable PTP-field coverage boundaries", authenticated_negative_request_builders},
        {"foreign Announce bytes and conservative time properties", foreign_announce_and_properties},
        {"restricted transmitter output policy", restricted_output_policy},
        {"current server GNSS PTP/TAI Announce compatibility", gnss_tai_announce_policy},
        {"sample statistics and observation classification", sample_statistics_and_classification},
        {"observation duplicates, reordering, sequence and history wrap", observation_duplicates_and_order},
        {"observation unmatched, expired and missing counters", observation_missing_and_expiration},
        {"generation changes and independent half-open windows", generations_and_independent_windows},
        {"bounded deterministic malformed corpus", malformed_corpus},
        {"versioned DUT policy and capability preflight", dut_policy_preflight},
        {"optional keyless/matching-key policy, observed mode, applicability and no downgrade", optional_authentication_policy},
        {"optional keyless rejection probes: exact valid/tampered Delay and Announce wire fixtures", optional_signed_probe_fixtures},
        {"PTP HMAC golden vectors, all signed bytes, negative mutations and downgrade prevention", authenticated_wire_vectors},
        {"independent ranking, boundaries and explicit Announce dataset", independent_ranking_and_builder},
        {"dataset changes and self-GM reflection: exact signed/unsigned same-source fixtures and boundaries", dataset_change_fixtures},
        {"dataset requalification and both overlapping-master expiry orders: decisive independent timing", dataset_and_overlap_timing},
        {"standby GNSS interaction: startup requalification, continuous polling and long-prompt lease cleanup", standby_timing_and_refresh},
        {"additional selection, authentication and invalid-refresh fixtures", additional_selection_and_authentication_fixtures},
        {"instrumented startup reference bounds and inconclusive timing", startup_reference_classification},
        {"ranking precedence: exact twelve-case wire matrix, field limits and reference DUT coverage", precedence_matrix_and_boundaries},
        {"legacy/ranked phase timing, source counters and final fresh expiry", foreign_phase_scheduling},
        {"sequence policy: exact signed/unsigned fixtures, qualification, rejected refresh and rollover timing", sequence_policy_fixtures_and_timing},
        {"interval policy: negative/long/unsupported signed fixtures and decisive ranked/legacy expiry deadlines", interval_policy_fixtures_and_timing},
        {"policy classification, cleanup, cancellation and unattended-first display ordering", policy_classification_and_ordering},
    };
    size_t failures = 0;
    for (const auto &test : cases)
    {
        try
        {
            test.run();
            std::printf("PASS: %s\n", test.name);
        }
        catch (const std::exception &error)
        {
            ++failures;
            std::fprintf(stderr, "FAIL: %s: %s\n", test.name, error.what());
        }
    }
    std::printf("%zu test groups, %zu assertions, %zu failures\n", std::size(cases), assertions, failures);
    return failures == 0 ? 0 : 1;
}

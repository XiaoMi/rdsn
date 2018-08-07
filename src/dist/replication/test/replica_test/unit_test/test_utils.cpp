#include <string>
#include <vector>
#include <map>
#include <gtest/gtest.h>

#include "test_utils.h"

uint32_t random32(uint32_t min, uint32_t max)
{
    uint32_t res = (uint32_t)(rand() % (max - min + 1));
    return res + min;
}

static const char CCH[] = "_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
static char buffer[256];
static std::map<std::string, std::map<std::string, std::string>> base;
static std::string expected_hash_key;

static const std::string random_string()
{
    int pos = random32(0, sizeof(buffer) - 1);
    buffer[pos] = CCH[random32(0, sizeof(CCH) - 1)];
    int length = random32(1, sizeof(buffer));
    if (pos + length < sizeof(buffer))
        return std::string(buffer + pos, length);
    else
        return std::string(buffer + pos, sizeof(buffer) - pos) +
               std::string(buffer, length + pos - sizeof(buffer));
}

dsn::blob generate_blob()
{
    std::string *data = new std::string();
    *data = random_string();
    std::shared_ptr<char> p((char *)data->data(), [data](char *p) { delete data; });
    return dsn::blob(std::move(p), data->size());
}

std::vector<dsn::replication::mutation_ptr> generate_mutations(int count, dsn::gpid gpid)
{
    std::vector<dsn::replication::mutation_ptr> ret;
    if (count <= 0) {
        return ret;
    }
    ret.resize(count);
    for (int i = 0; i < count; i++) {
        ret[i] = new dsn::replication::mutation();
        ret[i]->data.header.pid = gpid;
        ret[i]->set_id(i, i);
        ret[i]->set_timestamp(i + 1000000);
        int c = random32(0, 5);
        ret[i]->data.updates.resize(c);
        for (int j = 0; j < c; j++) {
            ret[i]->data.updates[j].code = LPC_WRITE_REPLICATION_LOG_COMMON;
            ret[i]->data.updates[j].data = generate_blob();
        }
        ret[i]->client_requests.resize(c);
    }
    return ret;
}

void compare_mutation(dsn::replication::mutation_ptr left, dsn::replication::mutation_ptr right)
{
    ASSERT_EQ((left)->get_ballot(), (right)->get_ballot());
    ASSERT_EQ((left)->get_decree(), (right)->get_decree());
    ASSERT_EQ((left)->data.header.timestamp, (right)->data.header.timestamp);
    ASSERT_EQ((left)->data.updates.size(), (right)->data.updates.size());
    for (int i = 0; i < (left)->data.updates.size(); i++) {
        ASSERT_EQ((left)->data.updates[i].data.length(), (right)->data.updates[i].data.length());
        ASSERT_FALSE(strncmp((left)->data.updates[i].data.data(),
                             (right)->data.updates[i].data.data(),
                             (left)->data.updates[i].data.length()));
    }
}

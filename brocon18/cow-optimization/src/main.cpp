#include <caf/all.hpp>

#include <benchmark/benchmark.h>

namespace {

using std::string;
using std::vector;

using namespace caf;

using cv = config_value;
using ls = cv::list;
using dc = cv::dictionary;

using data = cv;
using topic = string;

constexpr size_t num_messages = 10000;

cv make_data() {
  return cv{
      ls{cv{dc{{"keywords",
                cv{ls{cv{"actor model"}, cv{"pattern matching"},
                      cv{"distributed systems"}, cv{"native runtime"}}}}}},
         cv{"CAF is an open source C++11 actor model implementation featuring "
            "lightweight & fast actor implementations, pattern matching for "
            "messages, network transparent messaging, and more."}}};
}

using msg = std::pair<string, data>;

void source(event_based_actor* self, vector<actor> snks) {
  auto mgr = self->make_source(
    snks.front(),
    [](size_t& n) {
      n = 0;
    },
    [](size_t& n, downstream<msg>& out, size_t hint) {
      auto num = std::min(num_messages - n, hint);
      for (size_t i = 0; i < num; ++i)
        out.push(msg{"programming", make_data()});
      n += num;
    },
    [](const size_t& n) {
      return n == num_messages;
    }
  ).ptr();
  snks.erase(snks.begin());
  for (auto& snk : snks)
    mgr->add_unchecked_outbound_path<msg>(snk);
}

using cow_msg = std::shared_ptr<msg>;

} // namespace <anonymous>

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(cow_msg)

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(vector<cow_msg>)

namespace {

void cow_source(event_based_actor* self, vector<actor> snks) {
  auto mgr = self->make_source(
    snks.front(),
    [](size_t& n) {
      n = 0;
    },
    [](size_t& n, downstream<cow_msg>& out, size_t hint) {
      auto num = std::min(num_messages - n, hint);
      for (size_t i = 0; i < num; ++i)
        out.push(std::make_shared<msg>("programming", make_data()));
      n += num;
    },
    [](const size_t& n) {
      return n == num_messages;
    }
  ).ptr();
  snks.erase(snks.begin());
  for (auto& snk : snks)
    mgr->add_unchecked_outbound_path<cow_msg>(snk);
}

behavior sink(event_based_actor* self, actor cb) {
  return {
    [=](stream<msg> in) {
      self->make_sink(
        in,
        [](unit_t&) {
          // nop
        },
        [](unit_t&, msg) {
          // nop
        },
        [=](const unit_t&) {
          self->send(cb, ok_atom::value);
        }
      );
    },
    [=](stream<cow_msg> in) {
      self->make_sink(
        in,
        [](unit_t&) {
          // nop
        },
        [](unit_t&, cow_msg) {
          // nop
        },
        [=](const unit_t&) {
          self->send(cb, ok_atom::value);
        }
      );
    }
  };
}

struct fixture : benchmark::Fixture {
  actor_system_config cfg;
  actor_system sys;
  scoped_actor self;
  vector<actor> snks;
  vector<char> blob;
  cv raw;

  fixture() : sys(cfg), self(sys) {
    for (auto i = 0; i < 5; ++i)
      snks.emplace_back(sys.spawn(sink, self));
    blob.resize(264);
    raw = make_data();
  }

  void wait_till_done() {
    for (size_t i = 0; i < snks.size(); ++i)
      self->receive(
        [&](ok_atom res) {
          benchmark::DoNotOptimize(res);
        }
      );
  }

  void BenchmarkCase(benchmark::State&) {
    // nop
  }
};

} // namespace <anonymous>

BENCHMARK_F(fixture, ValueTest)(benchmark::State& state) {
  for (auto _ : state) {
    sys.spawn(source, snks);
    wait_till_done();
  }
}

BENCHMARK_F(fixture, PointerTest)(benchmark::State& state) {
  for (auto _ : state) {
    sys.spawn(cow_source, snks);
    wait_till_done();
  }
}

BENCHMARK_F(fixture, SerializeRaw)(benchmark::State& state) {
  for (auto _ : state) {
    vector<char> buf;
    binary_serializer bs{sys, buf};
    bs << raw;
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_F(fixture, SerializeBlob)(benchmark::State& state) {
  for (auto _ : state) {
    vector<char> buf;
    binary_serializer bs{sys, buf};
    bs << blob;
    benchmark::DoNotOptimize(buf);
  }
}

BENCHMARK_MAIN();


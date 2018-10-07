#include <caf/all.hpp>

#include <benchmark/benchmark.h>

using msg = std::pair<std::string, caf::config_value>;

using cow_msg = std::shared_ptr<msg>;

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(cow_msg)

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(std::vector<cow_msg>)

namespace {

using std::string;
using std::vector;

using namespace caf;

using cv = config_value;
using ls = cv::list;
using dc = cv::dictionary;

using data = cv;
using topic = string;

constexpr size_t num_messages = 100000;

cv make_data() {
  return cv{
      ls{cv{dc{{"keywords",
                cv{ls{cv{"actor model"}, cv{"pattern matching"},
                      cv{"distributed systems"}, cv{"native runtime"}}}}}},
         cv{"CAF is an open source C++11 actor model implementation featuring "
            "lightweight & fast actor implementations, pattern matching for "
            "messages, network transparent messaging, and more."}}};
}

template <class T>
T generate();

template <>
msg generate<msg>() {
  return msg{"programming", make_data()};
}

template <>
cow_msg generate<cow_msg>() {
  return std::make_shared<msg>("programming", make_data());
}

struct source_state {
  const char* name = "source";
};

template <class T>
void source(stateful_actor<source_state>* self, vector<actor> snks) {
  auto mgr = self->make_source(
    snks.front(),
    [](size_t& n) {
      n = 0;
    },
    [](size_t& n, downstream<T>& out, size_t hint) {
      auto num = std::min(num_messages - n, hint);
      for (size_t i = 0; i < num; ++i)
        out.push(generate<T>());
      n += num;
    },
    [](const size_t& n) {
      return n == num_messages;
    }
  ).ptr();
  for (auto i = snks.begin() + 1; i != snks.end(); ++i)
    mgr->add_outbound_path(*i);
}

struct sink_state {
  const char* name = "sink";
};

template <class T>
behavior sink(stateful_actor<sink_state>* self, actor cb) {
  return {
    [=](stream<T> in) {
      self->make_sink(
        in,
        [](unit_t&) {
          // nop
        },
        [](unit_t&, T) {
          // nop
        },
        [=](const unit_t&) {
          self->send(cb, ok_atom::value);
        }
      );
    }
  };
}

struct core_state {
  const char* name = "core";
};

template <class T>
behavior core(stateful_actor<core_state>* self, actor cb, int num_subs) {
  auto stage = self->make_continuous_stage(
    // initialize state
    [](unit_t&) {
      // nop
    },
    // processing step
    [](unit_t&, downstream<T>& out, T x) {
      out.push(std::move(x));
    },
    // cleanup
    [=](unit_t&, const error&) {
      self->send(cb, ok_atom::value);
    }
  );
  for (auto i = 0; i < num_subs; ++i)
    stage->add_outbound_path(self->spawn(sink<T>, cb));
  return {
    [=](stream<T> in) {
      stage->continuous(false);
      return stage->add_inbound_path(in);
    },
  };
}

struct fixture : benchmark::Fixture {
  actor_system_config cfg;
  actor_system sys;
  scoped_actor self;
  vector<char> blob;
  cv raw;

  fixture() : sys(cfg), self(sys) {
    blob.resize(264);
    raw = make_data();
  }
};

template <class T>
struct core_fixture : fixture {
  void run(benchmark::State& state) {
    int subs = state.range(0) ;
    for (auto _ : state) {
      sys.spawn(source<T>, vector<actor>{sys.spawn(core<T>, self, subs)});
      for (auto i = 0; i < subs + 1; ++i)
        self->receive(
          [&](ok_atom) {
            // nop
          }
        );
    }
  }
};

} // namespace <anonymous>

BENCHMARK_TEMPLATE_DEFINE_F(core_fixture, ValueTest, msg)(benchmark::State& state) {
  run(state);
}

BENCHMARK_REGISTER_F(core_fixture, ValueTest)
    ->Arg(1)
    ->Arg(2)
    ->Arg(3)
    ->Arg(4)
    ->Arg(5);

BENCHMARK_TEMPLATE_DEFINE_F(core_fixture, CowTest, cow_msg)(benchmark::State& state) {
  run(state);
}

BENCHMARK_REGISTER_F(core_fixture, CowTest)
    ->Arg(1)
    ->Arg(2)
    ->Arg(3)
    ->Arg(4)
    ->Arg(5);

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


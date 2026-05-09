#pragma once

#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <atomic>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io},
        gas_cooker_(std::make_shared<GasCooker>(io_)) {
    }

    void OrderHotDog(HotDogHandler handler) {
        auto sausage = store_.GetSausage();
        auto bread = store_.GetBread();

        struct State {
            std::shared_ptr<Sausage> sausage;
            std::shared_ptr<Bread> bread;
            bool sausage_ready = false;
            bool bread_ready = false;
        };

        auto state = std::make_shared<State>();
        state->sausage = sausage;
        state->bread = bread;

        auto try_finish = [this, state, handler]() {
            if (state->sausage_ready && state->bread_ready) {
                try {
                    HotDog hotdog(++next_hotdog_id_,
                                  state->sausage,
                                  state->bread);
                    handler(Result<HotDog>(std::move(hotdog)));
                } catch (...) {
                    handler(Result<HotDog>::FromCurrentException());
                }
            }
        };

        // СОСИСКА
        sausage->StartFry(*gas_cooker_, [this, sausage, state, try_finish]() {
            state->sausage_ready = true;
            sausage->StopFry();
            gas_cooker_->ReleaseBurner();
            try_finish();
        });

        // ХЛЕБ
        bread->StartBake(*gas_cooker_, [this, bread, state, try_finish]() {
            state->bread_ready = true;
            bread->StopBaking();
            gas_cooker_->ReleaseBurner();
            try_finish();
        });
    }

private:
    net::io_context& io_;

    Store store_;

    std::shared_ptr<GasCooker> gas_cooker_;

    std::atomic_int next_hotdog_id_ = 0;
};
// cafeteria.h

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    void OrderHotDog(HotDogHandler handler) {
        auto sausage = store_.GetSausage();
        auto bread = store_.GetBread();

        struct OrderState {
            std::shared_ptr<Sausage> sausage;
            std::shared_ptr<Bread> bread;

            bool sausage_ready = false;
            bool bread_ready = false;

            HotDogHandler handler;
        };

        auto state = std::make_shared<OrderState>(
            OrderState{sausage, bread, false, false, std::move(handler)});

        auto try_complete = [state]() {
            if (state->sausage_ready && state->bread_ready) {
                try {
                    HotDog hotdog{
                        state->sausage->GetId(),
                        state->sausage,
                        state->bread
                    };

                    state->handler(Result<HotDog>{std::move(hotdog)});
                } catch (...) {
                    state->handler(Result<HotDog>::FromCurrentException());
                }
            }
        };

        sausage->StartFry(*gas_cooker_, [this, sausage, state, try_complete] {
            auto timer = std::make_shared<net::steady_timer>(
                io_, HotDog::MIN_SAUSAGE_COOK_DURATION);

            timer->async_wait(
                [sausage, state, try_complete, timer](sys::error_code ec) {
                    if (ec) {
                        return;
                    }

                    try {
                        sausage->StopFry();
                        state->sausage_ready = true;
                        try_complete();
                    } catch (...) {
                        state->handler(Result<HotDog>::FromCurrentException());
                    }
                });
        });

        bread->StartBake(*gas_cooker_, [this, bread, state, try_complete] {
            auto timer = std::make_shared<net::steady_timer>(
                io_, HotDog::MIN_BREAD_COOK_DURATION);

            timer->async_wait(
                [bread, state, try_complete, timer](sys::error_code ec) {
                    if (ec) {
                        return;
                    }

                    try {
                        bread->StopBaking();
                        state->bread_ready = true;
                        try_complete();
                    } catch (...) {
                        state->handler(Result<HotDog>::FromCurrentException());
                    }
                });
        });
    }

private:
    net::io_context& io_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
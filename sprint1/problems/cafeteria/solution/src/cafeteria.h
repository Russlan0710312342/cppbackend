void OrderHotDog(HotDogHandler handler) {

    auto sausage = store_.GetSausage();
    auto bread = store_.GetBread();

    struct State {
        std::shared_ptr<Sausage> sausage;
        std::shared_ptr<Bread> bread;

        bool sausage_ready = false;
        bool bread_ready = false;
        bool done = false;
    };

    auto state = std::make_shared<State>();
    state->sausage = sausage;
    state->bread = bread;

    auto check_ready = [this, state, handler]() {

        if (state->done) return;

        if (state->sausage_ready && state->bread_ready) {
            state->done = true;

            try {
                HotDog hotdog(
                    ++next_hotdog_id_,
                    state->sausage,
                    state->bread
                    );

                handler(Result<HotDog>(std::move(hotdog)));
            }
            catch (...) {
                handler(Result<HotDog>::FromCurrentException());
            }
        }
    };

    // 🍖 сосиска
    sausage->StartFry(*gas_cooker_,
                      [sausage, state, check_ready]() {

                          sausage->StopFry();   // фиксируем время

                          state->sausage_ready = true;
                          check_ready();
                      }
                      );

    // 🥖 хлеб
    bread->StartBake(*gas_cooker_,
                     [bread, state, check_ready]() {

                         bread->StopBaking();  // фиксируем время

                         state->bread_ready = true;
                         check_ready();
                     }
                     );
}
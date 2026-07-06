# Dolphin Language — Kaam Baki Chha Report (Gap Analysis)

Yo file le "Dolphin" language haleko GitHub repo (Phuyalshankar/dolphin-language) kati banisakeko cha ra "world-class" language banauna ke ke garna baki cha bhanne kura note gareko cha.

## Aile ke cha (Current State)

- Yo ek **transpiler** ho — `.dolphin` file lekhera `.cpp` ma convert garincha, ani `g++` le compile garincha. Aafai bytecode/native compiler chaina, C++ compiler nai chai chai rahancha.
- Feature haru: variables, `if/else`, `while`, `loop`, functions (`fn`), lambda, list/array, object literal, string concatenation, comments, `import` (multi-file), basic microcontroller-style `Pin` simulation, TCP/HTTP server class, JSON helper, File helper, event loop (async callback jastai).
- Test file haru (`.dolphin`) dekhda features dherai try gareko dekhincha (CRUD, event-driven, functional style, IoT simulation, JS-jasto syntax).
- Runtime chai `dolphin_runtime.hpp` ek dherai thulo (~1600 line) single header file ho — sabai type (`var`) dynamic/variant type ho, C++ ko `var` class le number/string/list/object sabai handle garcha.

## Aaja fix gareko (already fixed in this session)

1. **Linux ma build hunna thiyo** — `closesocket` macro le member function `close()` sanga naam clash gardai thiyo, TCP socket close hunna thiyo. Fix gareko.
2. **CLI (`dolphin.cpp`) Windows-only thiyo** — `.exe`, `del` command hardcode thiyo, Linux/Mac ma chalna sakthiyeana. Cross-platform banaeko.
3. **Build system thiyeana** — `Makefile` thapeko (`make all`, `make run FILE=test.dolphin`).
4. **Real parser banayeko (regex → real lexer/AST/parser/codegen)** — purano line-by-line regex parsing hataera, proper pipeline banayeko:
   - `token.hpp` / `lexer.hpp/.cpp` — char-by-char tokenizer, line number tracking sahit.
   - `ast.hpp` — full Expr/Stmt node hierarchy.
   - `parser.hpp/.cpp` — recursive-descent + operator-precedence parser (sabai operator, control flow, `fn`/lambda, array/object literal, `import`, `pin` declaration/expression handle garcha).
   - `codegen.hpp/.cpp` — AST bata C++ code generate garcha (purano runtime call convention sangai compatible).
   - Faida: multi-line expression, nested block, complex syntax aba reliably parse huncha; error message aba line number sahit aucha (jasttai "Parse error at line 4: ...").
   - Trade-off: comment preservation (cosmetic matra thiyo) generated `.cpp` ma haraayo — correctness ma asar chaina.

Aba `make all && ./dolphin run test.dolphin` le Replit/Linux ma chalcha, real parser sanga.

## Real parser rewrite le pass gareko test suite (verified)

`test.dolphin`, `test_functions`, `test_loops`, `test_operators`, `test_functional`, `test_object_rotate`, `test_shifting`, `test_crud`, `test_import`, `test_files`, `test_complex`, `test_js`, `test_async`, `test_event_driven`, `test_server`, `test_iot_sim` — sabai transpile + compile + run huncha.

Yo pass garaudai thaha paayeko ra fix gareko sano runtime bug haru pani:
- `INPUT`/`OUTPUT` pin-mode constants naya parser ma translate hudaina thiyo (`PIN_INPUT`/`PIN_OUTPUT` ma map garna parcha) — fix gareko.
- Top-level (global scope) variable haru function bhitra bata access garda C++ compile error aunthyo (function le tyeslai global bhanera chinena) — top-level `var` haru aba real C++ global declare huncha.
- Lambda/function ko return statement haru le kahile `bool` kahile `var` return garda (`&&`/comparison jasto) C++ le "inconsistent lambda return type" error dinthyo — sabai return `var(...)` ma wrap gareko.
- `var::has()` ma ambiguous overload (`std::string` vs `var`) thiyo, string literal pass garda compile error — overload merge gareko.
- `pin(13, OUTPUT)` jasto expression (constructor call, declaration keyword hoina) parser ma chuttyaudinathiyo — fix gareko.

**Known limitation (runtime design, is session ma nachedeko)**: `pin` type le lambda closure bhitra by-value capture huncha (Pin class ma shared_ptr/reference semantics chaina jasto `var` ma cha), tesle garda closure bhitra `.write()` garda outer variable ma update dekhindaina. Yo fix garna Pin class lai `var`-jastai shared state (shared_ptr) ma convert garnu parcha — future work ma list gareko. (Yo purano regex-based transpiler ma pani thiyo, naya parser le nabanaeko naya bug haina.)

## World-Class Language Banauna Baki Kaam (Priority Order)

### 1. Foundation (jaruri, sabailai chainxa)
- [ ] **README.md** likhne — language ko intro, install, syntax example. Aile repo ma kunai documentation cha nai chaina.
- [ ] **Automated tests** — `.dolphin` test file haru chan tara "expected output" sanga automatically compare garne test runner chaina. Manually run garera herna parcha.
- [ ] **Error messages** — transpiler le error dekhauda line number/column, kaha galat vayo bhanne clarity chaina. Compiler error jasto user-friendly hunu parcha.
- [ ] **Regex-based parsing risky cha** — aile line-by-line regex le parse garincha (real AST/tokenizer chaina). Ramro multi-line expression, nested structure, complex syntax ma easily break huna sakcha. Real lexer→parser→AST pipeline banaunu long-term ma jaruri.

### 2. Language Correctness
- [ ] Type checking / static analysis chaina — sabai `var` (dynamic), so type error runtime samma pata lagdaina.
- [ ] Event loop bug cha — kehi async callback (e.g. `led.on("change", ...)`) properly trigger vayeko dekhina test run garda — timing/flush issue jasto.
- [ ] String/number auto-conversion edge case (jasttai `print(name + " version " + version)` ma `1.0` → `1` print vayo, decimal precision loss).

### 3. Tooling (developer experience)
- [ ] Package manager (dolphin libraries share/install garne tarika) chaina.
- [ ] Syntax highlighting / VSCode extension chaina.
- [ ] REPL (interactive shell) chaina — sabai file run garna transpile+compile garnu parcha, slow feedback loop.
- [ ] Standard library sano cha — Math, JSON, File, basic TCP/HTTP matra cha. String manipulation, date/time, regex, collections (map/set proper), error handling (try/catch) jasta features chaina.

### 4. Platform & Distribution
- [ ] Windows-centric thiyo, Linux/Mac support aaile matra manually add gareko — proper CI (GitHub Actions) le sabai OS ma automatically test/build garnu parcha.
- [ ] Binary distribution (installer, package for Linux/Mac/Windows) chaina — abhile source bata manually compile garnu parcha.
- [ ] Version/release process chaina (no versioning, no changelog, no releases tab).

### 5. Ecosystem (real "world-class" hunako lagi)
- [ ] Community/docs site chaina.
- [ ] Example projects/tutorials chaina.
- [ ] Security review (kunai bhi language jaba TCP/HTTP support garcha, tesle security concerns lyaucha — input validation, sandboxing) chaina.

## Summary — Kati % Baki Cha?

Yo **early prototype stage** ho (proof-of-concept). Core idea (custom syntax → C++ transpile) kaam garcha, tara "world-class language" (jasto Python, Go, Rust level) banauna:
- Foundation + tooling + docs + real parser: **dherai jaso kaam baki (~80-90%)**.
- Feature richness (jo aile test file ma dekhincha): ramro start, tara production-grade hunu lagi dherai polish chaina.

**Sabaibhanda pahila garna sakine kaam**: real lexer/parser/AST banaune, ani README + error messages thik parne. Tyo pachi standard library ra tooling ma focus garna milcha.

## Real Hardware Flashing (ESP32 / ESP8266 / STM32 / Arduino) — Naya Feature

Aile samma "run" command le sirf **PC simulation** garthyo (g++ le compile garera host machine ma chalaune, Pin class simulate matra thiyo). Aba `dolphin flash` command le **real microcontroller ma flash** garna milcha.

**Kasari kaam garcha:**
- `dolphin flash <file.dolphin> <board> <port>` — naya hardware codegen target (`dolphin_hardware_runtime.hpp`) use garera `.ino` sketch generate garcha (real `pinMode`/`digitalWrite`/`digitalRead`, simulated Pin hoina), ani `arduino-cli compile` + `arduino-cli upload` chalauncha.
- Top-level `loop { }` block feature bhaye tyo Arduino ko real `loop()` function ma jancha (repeatedly call huncha); baaki sabai code `setup()` ma euta patak matra chalcha.
- Supported boards: `esp32`, `esp8266`, `uno`, `nano`, `mega`, `stm32` (FQBN mapping `dolphin.cpp` ma cha).

**IMPORTANT — yo sirf local machine ma matra chalcha, Replit cloud ma hoina:**
- Replit ko cloud sandbox le USB/serial port access garna sakdaina, tesले flashing **VS Code ma tapaiko aafnai computer ma** garnu parcha (board USB le connect gareko avstha ma).
- Requirement: `arduino-cli` install garnu parcha (https://arduino.github.io/arduino-cli/latest/installation/), ani related board core install (e.g. `arduino-cli core install esp32:esp32`).
- `.vscode/tasks.json` ma "Dolphin: Flash to Board" task already cha — VS Code ma `Terminal > Run Task` bata board ra serial port select garera flash garna milcha (yo repo GitHub bata clone/pull garepachi).

**Known limitation:** stock Arduino Uno/Nano (plain AVR, avr-gcc) ma full C++ STL (std::string/std::function) support chaina, tesले `var` ko dynamic-typing runtime tyaha compile nahuna sakcha — best-effort matra. ESP32/ESP8266/STM32 ma full libstdc++ cha, tyo primary well-supported target ho.

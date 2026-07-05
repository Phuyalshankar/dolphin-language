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

Aba `make all && ./dolphin run test.dolphin` le Replit/Linux ma chalcha.

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

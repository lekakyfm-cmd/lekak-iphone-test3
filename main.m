#import <UIKit/UIKit.h>
#import <sys/mman.h>
#import <errno.h>
#import <stdint.h>
#import <unistd.h>
#import <string.h>
#import "address_adapter.h"
#import "engine_bridge.h"
#import "disc_loader.h"
#import "game_memory.h"
#import "guest_exec.h"
#import "startup_memory.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <stdlib.h>
#import <dispatch/dispatch.h>

/* This is a device compatibility probe, not the Lekak game. No ROM data,
 * dynamic executable allocation, code patching, or MAP_FIXED is used. */
static NSDictionary *CheckMapping(uintptr_t address, size_t length) {
    errno = 0;
    void *mapped = mmap((void *)address, length, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    BOOL exact = mapped != MAP_FAILED && (uintptr_t)mapped == address;
    int error = mapped == MAP_FAILED ? errno : 0;
    NSString *got = mapped == MAP_FAILED ? @"failed" :
        [NSString stringWithFormat:@"0x%llX", (unsigned long long)(uintptr_t)mapped];
    if (mapped != MAP_FAILED) {
        /* Access only the allocation returned by mmap, then release it. */
        volatile uint32_t *word = mapped;
        *word = 0x4C454B41u;
        exact = exact && *word == 0x4C454B41u;
        munmap(mapped, length);
    }
    return @{@"requested": [NSString stringWithFormat:@"0x%llX", (unsigned long long)address],
             @"bytes": @(length), @"exact": @(exact), @"returned": got, @"errno": @(error)};
}


static int AddOne(uint32_t value) { return (int)value + 1; }
static int DoubleValue(uint32_t value) { return (int)value * 2; }
static NSDictionary *CheckTranslation(void) {
    const size_t ramSize = 0x200000u, scratchSize = (size_t)sysconf(_SC_PAGESIZE);
    void *ram = mmap(NULL, ramSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    void *scratch = mmap(NULL, scratchSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ram == MAP_FAILED || scratch == MAP_FAILED) {
        if (ram != MAP_FAILED) munmap(ram, ramSize);
        if (scratch != MAP_FAILED) munmap(scratch, scratchSize);
        return @{@"passed": @NO, @"allocation_ok": @NO};
    }
    GuestRegion regions[] = {{0x80000000u, ramSize, ram}, {0x9F800000u, scratchSize, scratch}};
    CallbackEntry callbacks[] = {{0xC0000001u, AddOne}, {0xC0000002u, DoubleValue}};
    GuestRecord original = {0x80000100u, 0xC0000001u}, restored = {0};
    void *recordStorage = guest_resolve(regions, 2, 0x80000020u, sizeof(original));
    memcpy(recordStorage, &original, sizeof(original));
    memcpy(&restored, recordStorage, sizeof(restored));
    uint32_t value = 41, got = 0, scratchValue = 0;
    memcpy(guest_resolve(regions, 2, restored.data, sizeof(value)), &value, sizeof(value));
    memcpy(&got, guest_resolve(regions, 2, restored.data, sizeof(got)), sizeof(got));
    memcpy(guest_resolve(regions, 2, 0x9F800000u, sizeof(value)), &value, sizeof(value));
    memcpy(&scratchValue, guest_resolve(regions, 2, 0x9F800000u, sizeof(value)), sizeof(value));
    BOOL memoryOK = got == 41 && scratchValue == 41 && restored.callback == original.callback;
    BOOL boundsOK = !guest_resolve(regions, 2, 0x801FFFFFu, 4) &&
        !guest_resolve(regions, 2, 0xFFFFFFFFu, 4) &&
        !guest_resolve(regions, 2, 0x80000000u, SIZE_MAX) &&
        !guest_resolve(regions, 2, 0x70000000u, 1);
    int answer = 0, second = 0;
    BOOL callsOK = guest_dispatch(callbacks, 2, restored.callback, got, &answer) && answer == 42 &&
        guest_dispatch(callbacks, 2, 0xC0000002u, got, &second) && second == 82;
    BOOL invalidOK = !guest_dispatch(callbacks, 2, 0xC00000FFu, got, &answer);
    NSDictionary *result = @{@"passed": @(memoryOK && boundsOK && callsOK && invalidOK),
        @"allocation_ok": @YES, @"translated_memory_roundtrip": @(memoryOK),
        @"bounds_rejection": @(boundsOK), @"static_callback_dispatch": @(callsOK),
        @"unknown_callback_rejection": @(invalidOK), @"guest_record_bytes": @(sizeof(GuestRecord)),
        @"native_ram_address": [NSString stringWithFormat:@"0x%llX", (unsigned long long)(uintptr_t)ram],
        @"callback_result": @(answer), @"second_callback_result": @(second),
        @"integration": @"Prototype only. Existing engine G32 dereferences and callbacks are not converted."};
    munmap(ram, ramSize); munmap(scratch, scratchSize);
    return result;
}

static NSDictionary *CheckEngine(void) {
    LekakEngineResult r;
    BOOL passed = LekakEngine_Run(&r);
    return @{@"passed": @(passed), @"allocation": @(r.allocation),
        @"ram_aliases": @(r.memory_aliases), @"scratchpad_aliases": @(r.scratchpad_aliases),
        @"span_rejection": @(r.rejected_spans), @"ordering_table": @(r.ordering_table),
        @"packet_collection": @(r.packet_collection), @"packet_validation": @(r.packet_validation),
        @"software_gpu_pixels": @(r.rendered_pixels), @"gpu_invalid_rejection": @(r.gpu_invalid_rejection),
        @"callback_bank_dispatch": @(r.callback_bank_dispatch), @"rng": @(r.rng), @"gte_register_roundtrip": @(r.gte), @"snapshot_words": @(r.snapshot_words),
        @"framebuffer_hash": [NSString stringWithFormat:@"%08X", r.framebuffer_hash],
        @"red_pixel": @(r.red_pixel), @"blue_pixel": @(r.blue_pixel), @"violet_pixel": @(r.violet_pixel),
        @"scope": @"Genuine engine memory, OT, packets, software GPU, GTE and RNG; synthetic fixture. No game loop, disc or mod hooks.",
        @"disabled_optional_services": @[@"Texture dump", @"Texture replacement packs"]};
}
static UIImage *EnginePreview(void) {
    NSMutableData *rgba = [NSMutableData dataWithLength:320*240*4];
    LekakEngine_CopyRGBA(rgba.mutableBytes);
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)rgba);
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGImageRef frame = CGImageCreate(320, 240, 8, 32, 320*4, space,
        (CGBitmapInfo)kCGImageAlphaLast, provider, NULL, NO, kCGRenderingIntentDefault);
    UIImage *image = frame ? [UIImage imageWithCGImage:frame] : nil;
    if (frame) CGImageRelease(frame);
    CGColorSpaceRelease(space); CGDataProviderRelease(provider);
    return image;
}

static NSDictionary *CompilerCheck(void) {
    NSString *path = [[NSBundle mainBundle] pathForResource:@"compiler-check" ofType:@"json"];
    NSData *data = path ? [NSData dataWithContentsOfFile:path] : nil;
    id json = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL] : nil;
    return [json isKindOfClass:[NSDictionary class]] ? json : @{@"supported": @NO, @"status": @"missing build result"};
}

static NSDictionary *CheckGameMemory(void) {
    MemoriesMemory *memory = calloc(1, sizeof(*memory));
    if (!memory) return @{@"passed": @NO};
    memory->ram[0x100] = 0xFE; memory->ram[0x101] = 0xFF;
    memory->ram[0x104] = 1;
    int answer = 9;
    BOOL compare = Lekak_CompareS16(memory, 0x80000100, 0x80000104, &answer) && answer == -1;
    BOOL copy = Lekak_CopyWords(memory, 0x80000200, 0x80000100, 3) &&
        !memcmp(memory->ram+0x200, memory->ram+0x100, 4);
    BOOL fill = Lekak_FillMemory(memory, 0x80000300, 0x1AB, 5) &&
        memory->ram[0x300] == 0xAB && memory->ram[0x307] == 0xAB && memory->ram[0x308] == 0;
    BOOL bounds = !Lekak_CopyWords(memory, 0x801FFFFC, 0x80000100, 5) &&
        !Lekak_FillMemory(memory, 0x80000001, 0, 4);
    free(memory);
    return @{@"passed": @(compare && copy && fill && bounds), @"compare_s16": @(compare),
        @"copy_words": @(copy), @"fill_memory": @(fill), @"bounds_rejection": @(bounds),
        @"scope": @"Three adapted game utility routines; full game still not converted."};
}

static NSString *Hex32(uint32_t value) { return [NSString stringWithFormat:@"%08X",value]; }
static NSDictionary *ExecutionDetails(const LekakExecResult *r) {
    NSMutableArray *trace = [NSMutableArray array];
    uint32_t count = r->trace_count < LEKAK_EXEC_TRACE ? r->trace_count : LEKAK_EXEC_TRACE;
    uint32_t first = r->trace_count > LEKAK_EXEC_TRACE ? r->trace_count % LEKAK_EXEC_TRACE : 0;
    for (uint32_t i=0;i<count;i++) {
        unsigned index=(first+i)%LEKAK_EXEC_TRACE;
        [trace addObject:@{@"pc":Hex32(r->trace_pc[index]),@"instruction":Hex32(r->trace_ins[index])}];
    }
    NSMutableArray *registers = [NSMutableArray array];
    for (unsigned i=0;i<32;i++) [registers addObject:Hex32(r->registers[i])];
    return @{@"returned":@(r->returned),@"instructions":@(r->steps),@"stop_pc":Hex32(r->pc),
        @"instruction":Hex32(r->instruction),@"detail":Hex32(r->detail),
        @"reason":[NSString stringWithUTF8String:r->reason],@"registers":registers,@"trace":trace};
}
static NSDictionary *RunDiscExecution(MemoriesMemory *original,const LekakDiscResult *disc) {
    MemoriesMemory *native=malloc(sizeof(*native)), *guest=malloc(sizeof(*guest));
    if (!native || !guest) { free(native);free(guest);return @{@"status":@"failed",@"error":@"Execution RAM allocation failed"}; }
    const uint32_t addresses[]={0x800151B0u,0x800134B4u,0x80035A58u};
    NSArray *names=@[@"Fade_Init",@"Main_ClearFrameServiceCallbacks",@"Movie_ResetPlaybackState"];
    NSMutableArray *checks=[NSMutableArray array];
    uint32_t gp=disc->gp ? disc->gp : 0x8009AF08u;
    uint32_t sp=disc->stack_base ? disc->stack_base+disc->stack_bytes : 0x801FFF00u;
    BOOL all=YES;
    for(unsigned i=0;i<3;i++) {
        memcpy(native,original,sizeof(*native));memcpy(guest,original,sizeof(*guest));
        /* Nonzero initial state makes reset verification meaningful even if the
         * BIN's startup data already contains zeros. Keep unrelated bytes intact. */
        uint32_t seed_address=i==0?0x800E9EC8u:(i==1?0x800E9DB0u:0x8009B318u);
        size_t seed_length=i==0?0x28:(i==1?16:1);
        memset(Memories_Resolve(native,seed_address,seed_length,1),0xA5,seed_length);
        memset(Memories_Resolve(guest,seed_address,seed_length,1),0xA5,seed_length);
        if(i==0){native->ram[0x9B145]=guest->ram[0x9B145]=0xA5;}
        if(i==1){Memories_WriteLE32(native->ram+0x9B0B8,0xA5A5A5A5);Memories_WriteLE32(guest->ram+0x9B0B8,0xA5A5A5A5);}
        BOOL adapted=LekakStartup_Apply(native,i);
        LekakExecResult result;
        BOOL returned=LekakExec_Run(guest,addresses[i],gp,sp,100000,&result);
        /* Exclude scratch stack only. All other guest RAM and scratchpad must
         * match, including untouched neighbours and pointer storage. */
        BOOL same=adapted && returned && !memcmp(native->ram,guest->ram,0x1F0000) &&
            !memcmp(native->scratchpad,guest->scratchpad,MEMORIES_SCRATCHPAD_SIZE);
        all=all && same;
        [checks addObject:@{@"routine":names[i],@"guest_address":Hex32(addresses[i]),
            @"native_applied":@(adapted),@"ram_matches":@(same),@"execution":ExecutionDetails(&result)}];
    }
    memcpy(guest,original,sizeof(*guest));LekakExecResult boot;
    LekakExec_Run(guest,disc->entry,gp,sp,100000,&boot);
    NSDictionary *report=@{@"status":@"finished",@"startup_routines_match":@(all),
        @"routine_checks":checks,@"entry_execution":ExecutionDetails(&boot),
        @"gp_used":Hex32(gp),@"sp_used":Hex32(sp),@"instruction_budget":@100000,
        @"scope":@"Bounded execution diagnostics on private RAM copies; no BIOS, MMIO, CD, audio, full game loop or Lekak hooks. Unsupported services halt; no fake success.",
        @"original_loaded_ram_preserved":@YES};
    free(native);free(guest);return report;
}

@interface ProbeController : UIViewController <UIDocumentPickerDelegate> {
    MemoriesMemory *_discMemory;
}
@property(nonatomic, strong) NSMutableDictionary *report;
@property(nonatomic, strong) UITextView *textView;
@property(nonatomic, strong) UIBarButtonItem *chooseButton;
@end

@implementation ProbeController
- (void)dealloc { free(_discMemory); }
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    self.title = @"Lekak — test 6";
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithTitle:@"Partager" style:UIBarButtonItemStylePlain target:self action:@selector(shareReport:)];
    self.chooseButton = [[UIBarButtonItem alloc] initWithTitle:@"Choisir BIN"
        style:UIBarButtonItemStylePlain target:self action:@selector(chooseDisc:)];
    self.navigationItem.leftBarButtonItem = self.chooseButton;
    NSDictionary *engine = CheckEngine(), *routines = CheckGameMemory();
    uintptr_t function = (uintptr_t)&CheckMapping;
    self.report = [@{@"probe_version": @6, @"is_game": @NO,
        @"system_version": UIDevice.currentDevice.systemVersion,
        @"device_model": UIDevice.currentDevice.model,
        @"native_pointer_bytes": @(sizeof(void *)),
        @"native_function_address": [NSString stringWithFormat:@"0x%llX", (unsigned long long)function],
        @"native_function_fits_32bits": @(function <= UINT32_MAX),
        @"engine_core": engine, @"game_memory_routines": routines,
        @"address_adapter": CheckTranslation(), @"compiler": CompilerCheck(),
        @"memory_tests": @[CheckMapping(0x80000000u,0x200000u)],
        @"disc_loader": @{@"status": @"not_selected", @"executed": @NO},
        @"runtime_code_patching": @"Not attempted; static dispatch required.",
        @"scope": @"Portable engine subset, six adapted utility/startup routines, disc loader and bounded execution harness. Not a playable game; no Lekak hooks."} mutableCopy];
    self.textView = [UITextView new];
    self.textView.translatesAutoresizingMaskIntoConstraints = NO;
    self.textView.editable = NO;
    self.textView.textContainerInset = UIEdgeInsetsMake(18,18,24,18);
    [self.view addSubview:self.textView];
    [NSLayoutConstraint activateConstraints:@[
        [self.textView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [self.textView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [self.textView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.textView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]]];
    [self refreshText];
}
- (void)refreshText {
    NSDictionary *engine = self.report[@"engine_core"], *disc = self.report[@"disc_loader"];
    NSMutableString *text = [NSMutableString stringWithFormat:
        @"EXECUTION DU DISQUE — ESSAI 6\n\nLe jeu ne démarre pas encore. Cette version charge le BIN et exécute une partie de son code pour vérifier les routines adaptées et repérer le premier blocage du démarrage.\n\nMoteur graphique : %@\nEmpreinte image : %@\nRoutines mémoire du jeu : %@\n\n",
        [engine[@"passed"] boolValue] ? @"OK" : @"ECHEC", engine[@"framebuffer_hash"],
        [self.report[@"game_memory_routines"][@"passed"] boolValue] ? @"OK" : @"ECHEC"];
    if ([disc[@"loaded"] boolValue]) {
        [text appendFormat:@"BIN chargé : OK\nExécutable : SLUS_014.11\nDonnées chargées : %@ octets\nAdresse mémoire : %@\nPoint d’entrée : %@\nEmpreinte : %@\n\nLe code est testé sur des copies de la RAM. Le BIN chargé est conservé intact.\n", disc[@"load_bytes"],disc[@"load_address"],disc[@"entry"],disc[@"payload_hash"]];
    } else if (disc[@"error"]) {
        [text appendFormat:@"Chargement impossible : %@\n\n",disc[@"error"]];
    } else if ([disc[@"status"] isEqual:@"loading"]) {
        [text appendString:@"Lecture du BIN et test d’exécution en cours…\n\n"];
    } else {
        [text appendString:@"Appuie sur « Choisir BIN » et sélectionne le fichier BIN USA depuis Fichiers. Si le fichier est sur iCloud, son téléchargement peut prendre du temps.\n\n"];
    }
    NSDictionary *execution=disc[@"execution"];
    if(execution){
        NSDictionary *boot=execution[@"entry_execution"];
        [text appendFormat:@"\nComparaison des routines de démarrage : %@\n",
            [execution[@"startup_routines_match"] boolValue]?@"OK":@"A VERIFIER"];
        for(NSDictionary *check in execution[@"routine_checks"])
            [text appendFormat:@"%@ : %@\n",check[@"routine"],[check[@"ram_matches"] boolValue]?@"OK":@"A VERIFIER"];
        if(boot){
            [text appendFormat:@"\nDémarrage réel : %@ instructions\nArrêt à : %@\nMotif : %@\n\nCet arrêt repère le prochain service à adapter. Le jeu complet ne tourne pas encore.\n",
                boot[@"instructions"],boot[@"stop_pc"],boot[@"reason"]];
        } else [text appendFormat:@"%@\n",execution[@"error"]?:@"Test non terminé"];
    }
    [text appendString:@"Puis utilise « Partager » pour envoyer le rapport JSON. Le rapport contient seulement le résultat du test, pas ton BIN.\n\nENGLISH\nChoose your own USA BIN (MODE2/2352) or ISO (2048). This test compares adapted startup routines with bounded execution of actual disc code and records the first boot obstacle. It is not a playable game. Share the JSON report after loading. No disc data is included in the report.\n\nRendu de test du moteur :\n"];
    NSMutableAttributedString *content = [[NSMutableAttributedString alloc] initWithString:text
        attributes:@{NSFontAttributeName:[UIFont systemFontOfSize:16],NSForegroundColorAttributeName:UIColor.labelColor}];
    NSTextAttachment *preview = [NSTextAttachment new]; preview.image = EnginePreview();
    preview.bounds = CGRectMake(0,0,280,210);
    [content appendAttributedString:[NSAttributedString attributedStringWithAttachment:preview]];
    self.textView.attributedText = content;
}
- (void)chooseDisc:(UIBarButtonItem *)sender {
    (void)sender;
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[UTTypeItem] asCopy:NO];
    picker.delegate = self; picker.allowsMultipleSelection = NO;
    [self presentViewController:picker animated:YES completion:nil];
}
- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    (void)controller;
    NSURL *url = urls.firstObject; if (!url) return;
    self.chooseButton.enabled = NO;
    self.navigationItem.rightBarButtonItem.enabled = NO;
    self.report[@"disc_loader"] = @{@"status":@"loading",@"executed":@NO};
    [self refreshText];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{
        BOOL scoped = [url startAccessingSecurityScopedResource];
        __block MemoriesMemory *candidate = calloc(1,sizeof(MemoriesMemory));
        __block LekakDiscResult result = {0};
        __block BOOL loaded = NO;
        NSError *coordinationError = nil;
        if (candidate) {
            NSFileCoordinator *coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
            [coordinator coordinateReadingItemAtURL:url options:0 error:&coordinationError byAccessor:^(NSURL *readURL) {
                FILE *file = fopen(readURL.fileSystemRepresentation,"rb");
                loaded = LekakDisc_Load(file,candidate,&result);
                if (file) fclose(file);
            }];
        } else snprintf(result.error,sizeof(result.error),"Guest RAM allocation failed");
        if (scoped) [url stopAccessingSecurityScopedResource];
        NSString *error = coordinationError ? @"Impossible de lire ce fichier. Télécharge-le dans Fichiers puis réessaie." :
            [NSString stringWithUTF8String:result.error];
        NSDictionary *details = loaded ? @{@"status":@"loaded",@"loaded":@YES,@"executed":@YES,@"execution":RunDiscExecution(candidate,&result),
            @"sector_bytes":@(result.sector_bytes),@"executable_bytes":@(result.executable_bytes),
            @"load_bytes":@(result.load_bytes),
            @"load_address":[NSString stringWithFormat:@"%08X",result.load_address],
            @"entry":[NSString stringWithFormat:@"%08X",result.entry],
            @"gp":[NSString stringWithFormat:@"%08X",result.gp],
            @"stack_base":[NSString stringWithFormat:@"%08X",result.stack_base],
            @"payload_hash":[NSString stringWithFormat:@"%08X",result.payload_hash]} :
            @{@"status":@"failed",@"loaded":@NO,@"executed":@NO,@"error":error ?: @"Lecture impossible"};
        dispatch_async(dispatch_get_main_queue(), ^{
            if (loaded) { free(self->_discMemory); self->_discMemory=candidate; }
            else free(candidate);
            self.report[@"disc_loader"] = details;
            self.chooseButton.enabled = YES;
            self.navigationItem.rightBarButtonItem.enabled = YES;
            [self refreshText];
        });
    });
}
- (void)shareReport:(UIBarButtonItem *)sender {
    NSData *data = [NSJSONSerialization dataWithJSONObject:self.report options:NSJSONWritingPrettyPrinted error:NULL];
    NSDateFormatter *formatter = [NSDateFormatter new];
    formatter.locale = [[NSLocale alloc] initWithLocaleIdentifier:@"en_US_POSIX"];
    formatter.dateFormat = @"yyyyMMdd_HHmmss";
    NSString *name = [NSString stringWithFormat:@"Lekak_iOS_Report_06_%@.json",[formatter stringFromDate:NSDate.date]];
    NSURL *url = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:name]];
    if (!data || ![data writeToURL:url atomically:YES]) return;
    UIActivityViewController *activity = [[UIActivityViewController alloc] initWithActivityItems:@[url] applicationActivities:nil];
    activity.popoverPresentationController.barButtonItem = sender;
    [self presentViewController:activity animated:YES completion:nil];
}
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end
@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    (void)application; (void)options;
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[UINavigationController alloc] initWithRootViewController:[ProbeController new]];
    [self.window makeKeyAndVisible];
    return YES;
}
@end
int main(int argc, char *argv[]) {
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(AppDelegate.class)); }
}

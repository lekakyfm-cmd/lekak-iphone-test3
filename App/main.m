#import <UIKit/UIKit.h>
#import <sys/mman.h>
#import <errno.h>
#import <stdint.h>
#import <unistd.h>
#import <string.h>
#import "address_adapter.h"

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

static NSDictionary *CompilerCheck(void) {
    NSString *path = [[NSBundle mainBundle] pathForResource:@"compiler-check" ofType:@"json"];
    NSData *data = path ? [NSData dataWithContentsOfFile:path] : nil;
    id json = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:NULL] : nil;
    return [json isKindOfClass:[NSDictionary class]] ? json : @{@"supported": @NO, @"status": @"missing build result"};
}

@interface ProbeController : UIViewController
@property(nonatomic, strong) NSDictionary *report;
@end

@implementation ProbeController
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    self.title = @"Lekak — essai 3";
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithTitle:@"Partager" style:UIBarButtonItemStylePlain target:self action:@selector(shareReport:)];
    NSArray *mappings = @[CheckMapping(0x80000000u, 0x200000u),
                           CheckMapping(0x9F800000u, (size_t)sysconf(_SC_PAGESIZE)),
                           CheckMapping(0xC0000000u, (size_t)sysconf(_SC_PAGESIZE))];
    NSDictionary *translation = CheckTranslation();
    uintptr_t function = (uintptr_t)&CheckMapping;
    self.report = @{@"probe_version": @3, @"is_game": @NO,
        @"system_version": UIDevice.currentDevice.systemVersion,
        @"device_model": UIDevice.currentDevice.model,
        @"native_pointer_bytes": @(sizeof(void *)),
        @"native_function_address": [NSString stringWithFormat:@"0x%llX", (unsigned long long)function],
        @"native_function_fits_32bits": @(function <= UINT32_MAX),
        @"address_adapter": translation, @"compiler": CompilerCheck(), @"memory_tests": mappings,
        @"runtime_code_patching": @"Not attempted. The engine needs static dispatch on iOS.",
        @"scope": @"Compatibility diagnostics only. No game engine or disc included."};
    NSMutableString *text = [NSMutableString stringWithString:
        @"TROISIEME ESSAI iOS — ADRESSES TRADUITES\n\nCette application vérifie les obstacles au portage. Elle ne lance pas encore le jeu.\n\n"];
    [text appendFormat:@"iOS : %@\nPointeurs natifs : %zu octets\nCompilateur compatible avec les pointeurs 32 bits : %@\n\n",
        UIDevice.currentDevice.systemVersion, sizeof(void *),
        [CompilerCheck()[@"supported"] boolValue] ? @"oui" : @"non"];
    [text appendFormat:@"Adaptation des adresses : %@\nMemoire traduite : %@\nAppels de fonctions : %@\nControle des limites : %@\n\n",
        [translation[@"passed"] boolValue] ? @"REUSSIE" : @"ECHEC",
        [translation[@"translated_memory_roundtrip"] boolValue] ? @"OK" : @"ECHEC",
        [translation[@"static_callback_dispatch"] boolValue] ? @"OK" : @"ECHEC",
        [translation[@"bounds_rejection"] boolValue] ? @"OK" : @"ECHEC"];
    [text appendString:@"Ce prototype traduit explicitement les adresses du jeu et utilise une table de fonctions. Il faut encore convertir les acces du moteur avant de lancer le jeu.\n\n"];
    for (NSDictionary *entry in mappings) {
        [text appendFormat:@"Adresse %@ : %@\nAdresse obtenue : %@\n\n",
            entry[@"requested"], [entry[@"exact"] boolValue] ? @"disponible" : @"non obtenue par ce test",
            entry[@"returned"]];
    }
    [text appendString:@"Une adresse non obtenue indique que la stratégie actuelle doit être adaptée ; ce test ne prouve pas que tous les autres modes de mapping sont impossibles.\n\nPartager le rapport permet d’analyser le résultat. Aucun compte, identifiant personnel ou fichier du téléphone n’est lu.\n\nENGLISH\nThis is a compatibility probe, not the game. It checks the compiler's 32-bit stored-pointer support and safely requests the engine's low memory addresses without overwriting existing mappings. A failed hint allocation does not prove that every alternative mapping strategy is impossible.\n"];
    UITextView *view = [UITextView new];
    view.translatesAutoresizingMaskIntoConstraints = NO;
    view.editable = NO;
    view.font = [UIFont systemFontOfSize:16];
    view.text = text;
    view.textContainerInset = UIEdgeInsetsMake(18, 18, 24, 18);
    [self.view addSubview:view];
    [NSLayoutConstraint activateConstraints:@[
        [view.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [view.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
        [view.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [view.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]]];
}
- (void)shareReport:(UIBarButtonItem *)sender {
    NSData *data = [NSJSONSerialization dataWithJSONObject:self.report options:NSJSONWritingPrettyPrinted error:NULL];
    NSURL *url = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"Lekak_iOS_Report.json"]];
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

#import <UIKit/UIKit.h>
#import <sys/mman.h>
#import <errno.h>
#import <stdint.h>
#import <unistd.h>

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
    self.title = @"Lekak — essai iPhone";
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
        initWithTitle:@"Partager" style:UIBarButtonItemStylePlain target:self action:@selector(shareReport:)];
    NSArray *mappings = @[CheckMapping(0x80000000u, 0x200000u),
                           CheckMapping(0x9F800000u, (size_t)sysconf(_SC_PAGESIZE)),
                           CheckMapping(0xC0000000u, (size_t)sysconf(_SC_PAGESIZE))];
    uintptr_t function = (uintptr_t)&CheckMapping;
    self.report = @{@"probe_version": @2, @"is_game": @NO,
        @"system_version": UIDevice.currentDevice.systemVersion,
        @"device_model": UIDevice.currentDevice.model,
        @"native_pointer_bytes": @(sizeof(void *)),
        @"native_function_address": [NSString stringWithFormat:@"0x%llX", (unsigned long long)function],
        @"native_function_fits_32bits": @(function <= UINT32_MAX),
        @"compiler": CompilerCheck(), @"memory_tests": mappings,
        @"runtime_code_patching": @"Not attempted. The engine needs static dispatch on iOS.",
        @"scope": @"Compatibility diagnostics only. No game engine or disc included."};
    NSMutableString *text = [NSMutableString stringWithString:
        @"DEUXIEME ESSAI iOS — COMPILATEUR RECENT\n\nCette application vérifie les obstacles au portage. Elle ne lance pas encore le jeu.\n\n"];
    [text appendFormat:@"iOS : %@\nPointeurs natifs : %zu octets\nCompilateur compatible avec les pointeurs 32 bits : %@\n\n",
        UIDevice.currentDevice.systemVersion, sizeof(void *),
        [CompilerCheck()[@"supported"] boolValue] ? @"oui" : @"non"];
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

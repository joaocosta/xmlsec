/** 
 * XML Security standards test: XMLDSig
 * 
 * See Copyright for the status of this software.
 * 
 * Author: Aleksey Sanin <aleksey@aleksey.com>
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#ifdef XMLSEC_CRYPTO_OPENSSL
#include <openssl/err.h>
#include <openssl/x509.h>
#endif /* XMLSEC_CRYPTO_OPENSSL */

#ifndef XMLSEC_NO_XSLT
#include <libxslt/xslt.h>
#include <libxslt/extensions.h> 
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>
#include <libexslt/exslt.h>
#endif /* XMLSEC_NO_XSLT */

#include <xmlsec/xmlsec.h>
#include <xmlsec/xmltree.h>
#include <xmlsec/keys.h>
#include <xmlsec/keysmngr.h>
#include <xmlsec/transforms.h>
#include <xmlsec/xmldsig.h>
#include <xmlsec/xmlenc.h>
#include <xmlsec/debug.h>
#include <xmlsec/errors.h>
#include <xmlsec/crypto.h>

#include <xmlsec/openssl/evp.h>
#include <xmlsec/openssl/x509.h>


static const char copyright[] =
    "Written by Aleksey Sanin <aleksey@aleksey.com>.\n"
    "Copyright (C) 2002 Aleksey Sanin.\n"
    "This is free software: see the source for copying information.\n";

static const char bugs[] = 
    "To report bugs or get some help check XML Security Library home page:\n"
    "  http://www.aleksey.com/xmlsec\n";

static const char usage[] = 
    "Usage: xmlsec %s [<options>] <file> [<file> [ ... ]]\n";

static const char helpCommands[] =     
    "XMLSec commands are:\n"
    "  help                  display this help information and exit\n"
    "  help-<command>        display help information for <command> and exit\n"
    "  version               print version information and exit\n"
    "  keys                  keys XML file manipulation\n"
#ifndef XMLSEC_NO_XMLDSIG
    "  sign                  sign data and output XML document\n"
    "  verify                verify signed document\n"
#endif /* XMLSEC_NO_XMLDSIG */
#ifndef XMLSEC_NO_XMLENC
    "  encrypt               encrypt data and output XML document\n"
    "  decrypt               decrypt data from XML document\n"
#endif /* XMLSEC_NO_XMLENC */
    "\n";

static const char helpVersion[] = 
    "Usage: xmlsec version\n"
    "\n"
    "Prints version information and exits.\n"
    "\n";    

static const char helpKeys[] = 
    "Keys XML file manipulation. The result keys set is written to the file.\n"
    "\n"
    "Keys generation options:\n"
    "  --gen-hmac <name>     generate new 24 bytes HMAC key and set the key name\n"
    "  --gen-rsa <name>      generate new RSA key and set the key name\n"
    "  --gen-dsa <name>      generate new DSA key and set the key name\n"
    "  --gen-des3 <name>     generate new DES key and set the key name\n"
    "  --gen-aes128 <name>   generate new AES 128 key and set the key name\n"
    "  --gen-aes192 <name>   generate new AES 192 key and set the key name\n"
    "  --gen-aes256 <name>   generate new AES 256 key and set the key name\n"
    "\n";

static const char helpKeySelect[] = 
    "Key selection options:\n"
    "  --session-key-hmac    generate and use session 24 bytes HMAC key\n"
    "  --session-key-rsa     generate and use session RSA key\n"
    "  --session-key-dsa     generate and use session DSA key\n"
    "  --session-key-des3    generate and use session DES key\n"
    "  --session-key-aes128  generate and use session AES 128 key\n"
    "  --session-key-aes192  generate and use session AES 192 key\n"
    "  --session-key-aes256  generate and use session AES 256 key\n"
    "\n";



static const char helpSign[] = 
    "Signs data in the file and outputs document in \"XML Signature\" format.\n"
    "\n"
#ifndef XMLSEC_NO_XMLDSIG    
    "Signature options:\n"
    "  --output <filename>   write signed document to file <filename>\n"
    "  --ignore-manifests    do not process <Manifest> elements\n"
    "  --fake-signatures     disable actual signature calc for perf tests\n"
#else  /* XMLSEC_NO_XMLDSIG */
    "XML Digital Signatures support was disabled during compilation\n"
#endif /* XMLSEC_NO_XMLDSIG */    
    "\n";

static const char helpVerify[] = 
    "Verifies signed XML document in the file.\n"
    "\n"
#ifndef XMLSEC_NO_XMLDSIG    
    "Verification options:\n"
    "  --ignore-manifests    do not process <Manifest> elements\n"
    "  --print-result        print the result information\n"
    "  --print-references    store and print the pre-digested\n"
    "                        signature references\n"
    "  --print-manifests     store and print the pre-digested\n"
    "                        manifests references\n"
    "  --print-signature     store and print the pre-signated\n"
    "                        data (<SignedInfo> element)\n"
    "  --print-all           combination of the all \"--print-*\" options\n"
    "  --print-xml           print the result information in XML format\n"
    "  --print-to-file <file> print the result to file <file>\n"
    "  --fake-signatures     disable actual signature calc for perf tests\n"
#else  /* XMLSEC_NO_XMLDSIG */
    "XML Digital Signatures support was disabled during compilation\n"
#endif /* XMLSEC_NO_XMLDSIG */    
    "\n";

static const char helpEncrypt[] = 
    "Encrypts data and outputs document in \"XML Encryption\" format.\n"    
    "\n"
#ifndef XMLSEC_NO_XMLENC
    "Encryption options:\n"
    "  --output <filename>   write encrypted document to file <filename>\n"
    "  --binary <binary>     binary file to encrypt\n"
    "  --xml <file>          XML file to encrypt\n"
#else /* XMLSEC_NO_XMLENC */
    "XML Encryption support was disabled during compilation\n"
#endif /* XMLSEC_NO_XMLENC */    
    "\n";

static const char helpDecrypt[] =
    "Decrypts data from document in \"XML Encryption\" format.\n"
#ifndef XMLSEC_NO_XMLENC
    "  --output <filename>   write decrypted document to file <filename>\n"
#else /* XMLSEC_NO_XMLENC */
    "\n"
    "XML Encryption support was disabled during compilation\n"
#endif /* XMLSEC_NO_XMLENC */    
    "\n";

static const char helpNodeSelection[] = 
    "Start node selection options:\n"
    "  --node-id <id>        set the operation start point to the node \n"
    "                        with given <id>\n"
    "  --node-name [<namespace-uri>:]<name>\n"
    "                        set the operation start point to the first node \n"
    "                        with given <name> and <namespace> URI\n"
    "\n";
    
static const char helpKeysMngmt[] = 
    "Keys management options:\n"
    "  --keys <file>         load keys from XML file\n"
    "  --privkey[:<name>] <file>[,<cafile>[,<cafile>[...]]]\n"
    "                        load private key from PEM file and certificates\n"
    "                        that verify this key\n"
    "  --pubkey[:<name>] <file>\n"
    "                        load public key from PEM file\n"
#ifndef XMLSEC_NO_X509
    "  --pkcs12[:<name>] <file>\n"
    "                        load private key from pkcs12 file\n"
#endif /* XMLSEC_NO_X509 */    
#ifndef XMLSEC_NO_HMAC    
    "  --hmackey[:<name>] <file>\n"
    "                        load hmac key from binary file\n"
#endif  /* XMLSEC_NO_HMAC */    
    "  --allowed <list>      specify the set of the allowed key origins\n"
    "                        for signature verification or decryption;\n"
    "                        <list> is a comma separated collection of\n"
    "                        the following values:\n"
    "                          \"keymanager\", \"keyname\", \"keyvalue\",\n"
    "                          \"retrieval-doc\", \"retrieval-remote\",\n"
    "                          \"enc-key\", \"x509\", \"pgp\"\n"
    "                        by default, all key origins are allowed\n"
    "  --pwd <password>      the password to use for reading keys and certs\n"
    "\n";
    
static const char helpX509[] =
#ifndef XMLSEC_NO_X509    
    "X509 certificates options:\n"
    "  --trusted <file>      load trusted (root) certificate from PEM file\n"
    "  --untrusted <file>    load un-trusted certificate from PEM file\n"
    "  --pwd <password>      the password to use for reading keys and certs\n"
    "  --verification-time <time> the local time in \"YYYY-MM-DD HH:MM:SS\"\n"
    "                       format used certificates verification\n"
#else /* XMLSEC_NO_X509 */
    "x509 certificates support was disabled during compilation\n"
#endif /* XMLSEC_NO_X509 */        
    "\n";
    
static const char helpMisc[] = 
    "Misc. options:\n"
    "  --repeat <number>     repeat the operation <number> times\n"
    "  --disable-error-msgs  do not print xmlsec error messages\n"
#ifdef XMLSEC_CRYPTO_OPENSSL
    "  --print-openssl-errors print openssl errors stack at the end\n"
#endif /* XMLSEC_CRYPTO_OPENSSL */
    "\n";

typedef enum _xmlsecCommand {
    xmlsecCommandNone = 0,
    xmlsecCommandKeys,
    xmlsecCommandSign,
    xmlsecCommandVerify,
    xmlsecCommandEncrypt,
    xmlsecCommandDecrypt
} xmlsecCommand;

typedef struct _xmlSecDSigStatus {
    size_t			signaturesOk;
    size_t			signaturesFail;
    size_t			signRefOk;
    size_t			signRefFail;
    size_t			manifestRefOk;
    size_t			manifestRefFail;
} xmlSecDSigStatus, *xmlSecDSigStatusPtr;

/**
 * Init/Shutdown
 */
int  initXmlsec(xmlsecCommand command);
void shutdownXmlsec(void);


/**
 * Read command line options
 */
int  readKeyOrigins(char *keyOrigins);
int  readPEMCertificate(const char *file, int trusted);
int  readNumber(const char *str, int *number);
int  readTime(const char* str, time_t* t);
int  readKeys(char *file);
int  readPemKey(int privateKey, char *param, char *name);
int  readHmacKey(char *filename, char *name);
int  readPKCS12Key(char *filename, char *name);

/**
 * Keys generation/manipulation
 */
xmlSecKeyPtr genKey(const char* type, const char *name);
 
/**
 * Print help
 */
void printUsage(const char *command);
void printVersion(void);


/**
 * XML Signature
 */
#ifndef XMLSEC_NO_XMLDSIG
xmlSecDSigCtxPtr dsigCtx = NULL;

void getDSigResult(xmlSecDSigResultPtr result, xmlSecDSigStatusPtr status);
int  generateDSig(xmlDocPtr doc);
int  validateDSig(xmlDocPtr doc);
#endif /* XMLSEC_NO_XMLDSIG */

/**
 * XML Encryption
 */
#ifndef XMLSEC_NO_XMLENC
char *data = NULL;
int binary = 0;
xmlSecEncCtxPtr encCtx = NULL;

int encrypt(xmlDocPtr tmpl);
int decrypt(xmlDocPtr doc);
#endif /* XMLSEC_NO_XMLENC */

/**
 * Global data
 */
xmlSecKeysMngrPtr keysMngr = NULL; 
xmlSecKeysMngrCtxPtr keysMngrCtx = NULL; 
xmlSecKeyPtr sessionKey = NULL;

char *output = NULL; 
char *nodeId = NULL;
char *nodeName = NULL;
char *nodeNs = NULL;
int repeats = 1;
int printResult = 0;
int printXml = 0;
FILE* printFile = NULL;
clock_t total_time = 0;
char *global_pwd = NULL;
int print_errors_stack = 0;

int main(int argc, char **argv) {
    int res = 1;
    xmlsecCommand command = xmlsecCommandNone;
    xmlDocPtr doc = NULL;
    int i;
    int pos;
    int ret;
    int templateRequired = 0;
        
    /**
     * Read the command
     */
    if((argc < 2) || (strcmp(argv[1], "help") == 0) || (strcmp(argv[1], "--help") == 0)) {
	printUsage(NULL);
	return(0);
    } else if(strncmp(argv[1], "help-", 5) == 0) { 
	printUsage(argv[1] + 5);
	return(0);
    } else if((strcmp(argv[1], "version") == 0) || (strcmp(argv[1], "--version") == 0)) {
	printVersion();
	return(0);
    } else if(strcmp(argv[1], "keys") == 0) {
	command = xmlsecCommandKeys;
#ifndef XMLSEC_NO_XMLDSIG
    } else if(strcmp(argv[1], "sign") == 0) {
	command = xmlsecCommandSign;
	templateRequired = 1;
    } else if(strcmp(argv[1], "verify") == 0) {
	command = xmlsecCommandVerify;
	templateRequired = 1;
#endif /* XMLSEC_NO_XMLDSIG */
#ifndef XMLSEC_NO_XMLENC
    } else if(strcmp(argv[1], "encrypt") == 0) {
	command = xmlsecCommandEncrypt;
	templateRequired = 1;
    } else if(strcmp(argv[1], "decrypt") == 0) {
	command = xmlsecCommandDecrypt;
	templateRequired = 1;
#endif /* XMLSEC_NO_XMLENC */
    } else if(strcmp(argv[1], "keys") == 0) {
	command = xmlsecCommandKeys;
    } else {
	fprintf(stdout, "Error: unknown command \"%s\"\n", argv[1]);
	printUsage(NULL);
	return(0);
    }
    
    ret = initXmlsec(command);
    if(ret < 0) {
	fprintf(stdout, "Error: init failed\n");
	goto done;
    }

    xmlSecTimerInit();
        
    ret = 0;
    pos = 2;
    while((pos < argc) && (argv[pos][0] == '-')) {
	/** 
	 * Node selection options 
	 */
	if((strcmp(argv[pos], "--node-id") == 0) && (pos + 1 < argc)) {    
	    if((nodeName != NULL) || (nodeId != NULL)){
		fprintf(stderr, "Error: another node selection option present\n");
		ret = -1;
	    } else {
		nodeId = argv[++pos];
	    }
	} else if((strcmp(argv[pos], "--node-name") == 0) && (pos + 1 < argc)) {    
	    if((nodeName != NULL) || (nodeId != NULL)){
		fprintf(stderr, "Error: another node selection option present\n");
		ret = -1;
	    } else {
		nodeName = strrchr(argv[++pos], ':');
		if(nodeName != NULL) {
		    *(nodeName++) = '\0';
		    nodeNs = argv[pos];
		} else {
		    nodeName = argv[pos];
		    nodeNs = NULL;
		}
	    }
	} else 

	/**
	 * Keys Mgmt options
	 */ 
	if((strcmp(argv[pos], "--keys") == 0) && (pos + 1 < argc)) {
	    ret = readKeys(argv[++pos]);
	} else if((strncmp(argv[pos], "--privkey", 9) == 0) && (pos + 1 < argc)) {
	    char *name;
	    
	    name = strchr(argv[pos], ':');
	    if(name != NULL) ++name;
	    ret = readPemKey(1, argv[++pos], name); 
	} else if((strncmp(argv[pos], "--pubkey", 8) == 0) && (pos + 1 < argc)) {
	    char *name;
	    
	    name = strchr(argv[pos], ':');
	    if(name != NULL) ++name;
	    ret = readPemKey(0, argv[++pos], name); 
	} else if((strncmp(argv[pos], "--pkcs12", 8) == 0) && (pos + 1 < argc)) {
	    char *name;
	    
	    name = strchr(argv[pos], ':');
	    if(name != NULL) ++name;	    
	    ret = readPKCS12Key(argv[++pos], name); 
	} else if((strncmp(argv[pos], "--hmackey", 9) == 0) && (pos + 1 < argc)) {
	    char *name;
	    
	    name = strchr(argv[pos], ':');
	    if(name != NULL) ++name;
	    ret = readHmacKey(argv[++pos], name); 
	} else if((strcmp(argv[pos], "--allowed") == 0) && (pos + 1 < argc)) {
	    ret = readKeyOrigins(argv[++pos]);
	} else 


	/**
	 * Key selection options
	 */	
	if((strncmp(argv[pos], "--session-key-", 14) == 0)) {
	    if(sessionKey != NULL) {
		fprintf(stderr, "Error: session key already selected\n");
		ret = -1;
	    } else {    
		sessionKey = genKey(argv[pos] + 14, NULL);
		if(sessionKey == NULL) {
		    ret = -1;
		}
	    }
	} else
	
	/**
	 * X509 certificates options
	 */
	if((strcmp(argv[pos], "--trusted") == 0) && (pos + 1 < argc)) {
	    ret = readPEMCertificate(argv[++pos], 1);
	} else if((strcmp(argv[pos], "--untrusted") == 0) && (pos + 1 < argc)) {	
	    ret = readPEMCertificate(argv[++pos], 0);
	} else if((strcmp(argv[pos], "--verification-time") == 0) && (pos + 1 < argc)) {
	    time_t t = 0;
	     
	    if(readTime(argv[++pos], &t) >= 0) {
		if(keysMngrCtx != NULL) {	    
		    keysMngrCtx->certsVerificationTime = t;
		}
    		ret = 0;
	    } else {
    		ret = -1;
	    }
	} else 

	/**
	 * Misc. options
	 */	
	if((strcmp(argv[pos], "--repeat") == 0) && (pos + 1 < argc)) {
	    ret = readNumber(argv[++pos], &repeats);
	} else if((strcmp(argv[pos], "--pwd") == 0) && (pos + 1 < argc)) {
	    global_pwd = argv[++pos];
	    ret = 0;
	} else if((strcmp(argv[pos], "--output") == 0) && (pos + 1 < argc)) {
	    output = argv[++pos];
	    ret = 0;
	} else if((strcmp(argv[pos], "--disable-error-msgs") == 0)) {
	    xmlSecPrintErrorMessages = 0;
	    ret = 0;
#ifdef XMLSEC_CRYPTO_OPENSSL
	} else if((strcmp(argv[pos], "--print-openssl-errors") == 0)) {
	    print_errors_stack = 1;
	    ret = 0;
#endif /* XMLSEC_CRYPTO_OPENSSL */
	} else 

	/**
	 * Keys options
	 */	
	if((strncmp(argv[pos], "--gen-", 6) == 0) && (pos + 1 < argc)) {
	    xmlSecKeyPtr key;
	    char* type = argv[pos] + 6;
	    key = genKey(type, argv[++pos]);
	    if(key != NULL) {
		ret = xmlSecSimpleKeysMngrAddKey(xmlSecSimpleKeysMngrCast(keysMngr), key);
	    } else {
		/* we ignore this error because it might be cause 
		by a missed algorithm */
		ret = 0;
	    }
	} else 
	
	
#ifndef XMLSEC_NO_XMLDSIG	
	/**
	 * Signature options
	 */
	if((strcmp(argv[pos], "--ignore-manifests") == 0) && (dsigCtx != NULL)) {
	    dsigCtx->processManifests = 0; 
	} else if((strcmp(argv[pos], "--fake-signatures") == 0) && (dsigCtx != NULL)) {
	    dsigCtx->fakeSignatures = 1; 
	} else 
	
	
	/**
	 * Verification  options
	 */
	if((strcmp(argv[pos], "--print-result") == 0) && (dsigCtx != NULL))  {
	    printResult = 1;
	} else if((strcmp(argv[pos], "--print-references") == 0) && (dsigCtx != NULL)) {
	    dsigCtx->storeReferences = 1; 
	    printResult = 1;	    
	} else if((strcmp(argv[pos], "--print-manifests") == 0) && (dsigCtx != NULL)) {
	    dsigCtx->storeManifests = 1; 
	    printResult = 1;
	} else if((strcmp(argv[pos], "--print-signature") == 0) && (dsigCtx != NULL)) {
	    dsigCtx->storeSignatures = 1; 
	    printResult = 1;
	} else if((strcmp(argv[pos], "--print-all") == 0) && (dsigCtx != NULL)) {	    
	    dsigCtx->storeReferences = 1; 
	    dsigCtx->storeManifests = 1; 
	    dsigCtx->storeSignatures = 1; 	    
	    printResult = 1;
	} else if((strcmp(argv[pos], "--print-xml") == 0) && (dsigCtx != NULL))  {
	    printXml = 1;
	    printResult = 1;
	} else if((strcmp(argv[pos], "--print-to-file") == 0) && (dsigCtx != NULL) && 
		  (pos + 1 < argc) && (printFile == NULL)) {
	    printFile = fopen(argv[++pos], "w");
	    if(printFile == NULL) {
		fprintf(stderr, "Error: failed to open result file \"%s\"\n", argv[pos]);
		ret = -1;
	    }
	} else 
#endif /* XMLSEC_NO_XMLDSIG */

#ifndef XMLSEC_NO_XMLENC
	
	/** 
	 * Encryption options
	 */
	if((strcmp(argv[pos], "--binary") == 0) && (pos + 1 < argc)) {
	    if(data != NULL){
		fprintf(stderr, "Error: data file was already specified\n");
		ret = -1;
	    } else {
		data = argv[++pos];
		binary = 1;
	    }
	} else if((strcmp(argv[pos], "--xml") == 0) && (pos + 1 < argc)) {
	    if(data != NULL){
		fprintf(stderr, "Error: data file was already specified\n");
		ret = -1;
	    } else {
		data = argv[++pos];
		binary = 0;
	    }
	} else 
	
	/**
	 * Decryption options
	 */
	if((strcmp(argv[pos], "") == 0) && (pos + 1 < argc)) {
	} else 
	
#endif /* XMLSEC_NO_XMLENC */

	/**
	 * Unknown option error
	 */
	{
	    fprintf(stderr, "Error: option \"%s\" is unknown\n", argv[pos]);
	    ret = -1;
	}
	
	/** 
	 * Check for error
	 */
	if(ret < 0) {
	    printUsage(argv[1]);
	    goto done;	    
	}
	++pos;
    }
    
    
    /**
     * Now process files one after another
     */
    ret = 0; 
    while((pos < argc) && (ret >= 0)) {
	templateRequired = 0;
	for(i = 0; (i < repeats); ++i) {
	    if(command == xmlsecCommandKeys) {
		/* simply save keys */
		ret = xmlSecSimpleKeysMngrSave(xmlSecSimpleKeysMngrCast(keysMngr), argv[pos]);
	    } else {
		doc = xmlSecParseFile(argv[pos]);
	        if(doc == NULL) {
		    fprintf(stderr, "Error: failed to read XML file \"%s\"\n", argv[pos]);
		    printUsage(argv[1]);
	    	    goto done;
		}
	
    		switch(command) {
	    
#ifndef XMLSEC_NO_XMLDSIG	    
    		case xmlsecCommandSign:
		    ret = generateDSig(doc);
		    break;
		case xmlsecCommandVerify:
		    ret = validateDSig(doc);
	    	    break;
#endif /* XMLSEC_NO_XMLDSIG */
		
#ifndef XMLSEC_NO_XMLENC
		case xmlsecCommandEncrypt:
		    ret = encrypt(doc);
		    break;
		case xmlsecCommandDecrypt:
		    ret = decrypt(doc);
		    break;
#endif /* XMLSEC_NO_XMLENC */

		default:
		    fprintf(stderr, "Error: unknown command\n");
	    	    printUsage(argv[1]);
		    goto done;	    
		}
	    }
	    if((ret < 0) && (repeats <= 1)) {				
		fprintf(stderr, "Error: operation failed\n");
 		goto done;	    	    
	    }
	    xmlFreeDoc(doc); doc = NULL;
	}
	++pos;
    }
    if(templateRequired != 0) {
	fprintf(stderr, "Error: no templates specified\n");
 	goto done;	    	    
    }
    
    if(repeats > 1) {
        fprintf(stderr, "Executed %d tests in %ld msec\n", repeats, total_time / (CLOCKS_PER_SEC / 1000));    
	if(xmlSecTimerGet() > 0.0001) {
	    fprintf(stderr, "The debug timer is %f\n", xmlSecTimerGet());    
	}
    }

    /* success */
    res = 0;
    
done:    
#ifdef XMLSEC_CRYPTO_OPENSSL
    if(print_errors_stack) {
	ERR_print_errors_fp(stderr);
    }
#endif /* XMLSEC_CRYPTO_OPENSSL */
    if(doc != NULL) {
	xmlFreeDoc(doc); 
    }
    shutdownXmlsec();
    return(res);
}

void printUsage(const char *command) {
    if(command == NULL) {
	fprintf(stderr, usage, "<command>");
	fprintf(stderr, "\n");
	fprintf(stderr, "%s", helpCommands);
    } else if(strcmp(command, "version") == 0) {
	fprintf(stderr, "%s", helpVersion);
    } else if(strcmp(command, "keys") == 0) {
	fprintf(stderr, usage, command);
	fprintf(stderr, "\n");
	fprintf(stderr, "%s", helpKeys);
	fprintf(stderr, "%s", helpKeysMngmt);
	fprintf(stderr, "%s", helpMisc);
    } else if(strcmp(command, "sign") == 0) {
	fprintf(stderr, usage, command);
	fprintf(stderr, "\n");
	fprintf(stderr, "%s", helpSign);
	fprintf(stderr, "%s", helpNodeSelection);
	fprintf(stderr, "%s", helpKeysMngmt);
	fprintf(stderr, "%s", helpKeySelect);
	fprintf(stderr, "%s", helpMisc);
    } else if(strcmp(command, "verify") == 0) {
	fprintf(stderr, usage, command);
	fprintf(stderr, "\n");
	fprintf(stderr, "%s", helpVerify);
	fprintf(stderr, "%s", helpNodeSelection);
	fprintf(stderr, "%s", helpKeysMngmt);
	fprintf(stderr, "%s", helpKeySelect);
	fprintf(stderr, "%s", helpX509);
	fprintf(stderr, "%s", helpMisc);
    } else if(strcmp(command, "encrypt") == 0) {
	fprintf(stderr, usage, command);
	fprintf(stderr, "\n");
	fprintf(stderr, "%s", helpEncrypt);
	fprintf(stderr, "%s", helpNodeSelection);
	fprintf(stderr, "%s", helpKeysMngmt);
	fprintf(stderr, "%s", helpKeySelect);
	fprintf(stderr, "%s", helpMisc);
    } else if(strcmp(command, "decrypt") == 0) {
	fprintf(stderr, usage, command);
	fprintf(stderr, "\n");
	fprintf(stderr, "%s", helpDecrypt);
	fprintf(stderr, "%s", helpNodeSelection);
	fprintf(stderr, "%s", helpKeysMngmt);
	fprintf(stderr, "%s", helpKeySelect);
	fprintf(stderr, "%s", helpX509);
	fprintf(stderr, "%s", helpMisc);
    } else {
	fprintf(stderr, "Error: unknown command \"%s\"\n", command);
	fprintf(stderr, usage, "<command>");
	fprintf(stderr, "\n");
	fprintf(stderr, "%s", helpCommands);
    }
    fprintf(stderr, "%s\n", bugs);
    fprintf(stderr, "%s\n", copyright);
}

void printVersion(void) {
    fprintf(stdout, "xmlsec %s\n", XMLSEC_VERSION);
    fprintf(stderr, "\n");
    fprintf(stderr, "%s\n", bugs);
    fprintf(stderr, "%s\n", copyright);    
}


/**
 * Init/Shutdown
 */
int initXmlsec(xmlsecCommand command) {
    /* 
     * Init crypto engine
     */
    if(xmlSecCryptoAppInit() < 0) { 
	fprintf(stderr, "Failed to initialize crypto engine\n"); 
	return(-1);
    }
    
    /*
     * Init libxml
     */     
    xmlInitParser();
    LIBXML_TEST_VERSION

    xmlTreeIndentString = "\t";
#ifndef XMLSEC_NO_XSLT
    xmlIndentTreeOutput = 1; 
#endif /* XMLSEC_NO_XSLT */
    
    /*
     * Init xmlsec
     */
    if(xmlSecInit() < 0) {
    	fprintf(stderr, "Error: failed to initialize xmlsec\n");
	return(-1);
    }

    /** 
     * Create Keys and x509 managers
     */
    keysMngr = xmlSecSimpleKeysMngrCreate();    
    if(keysMngr == NULL) {
	fprintf(stderr, "Error: failed to create keys manager\n");
	return(-1);
    }
    keysMngrCtx = xmlSecKeysMngrCtxCreate(keysMngr);    
    if(keysMngrCtx == NULL) {
	fprintf(stderr, "Error: failed to create keys manager context\n");
	return(-1);
    }

    switch(command) {
    
#ifndef XMLSEC_NO_XMLDSIG    
    case xmlsecCommandSign:
    case xmlsecCommandVerify:
	/**
	 * Init DSig context
         */    
        dsigCtx = xmlSecDSigCtxCreate(keysMngrCtx);
	if(dsigCtx == NULL) {
    	    fprintf(stderr,"Error: failed to create DSig context\n");
	    return(-1);
	}
        /**
	 * Set default values to process manifests and store nothing
         * Overwrite this options thru command line if needed!
	 */
        dsigCtx->processManifests = 1;
	dsigCtx->storeSignatures = 0;
	dsigCtx->storeReferences = 0;
        dsigCtx->storeManifests = 0;
	break;
#endif /* XMLSEC_NO_XMLDSIG */
	
#ifndef XMLSEC_NO_XMLENC
    case xmlsecCommandEncrypt:
    case xmlsecCommandDecrypt:
        encCtx = xmlSecEncCtxCreate(keysMngrCtx);
	if(encCtx == NULL) {
    	    fprintf(stderr,"Error: failed to create Enc context\n");
	    return(-1);
	}	
	break;
#endif /* XMLSEC_NO_XMLENC */
    default:
	break;
    }	
    return(0);    
}

void shutdownXmlsec(void) {

    if(sessionKey != NULL) {
	xmlSecKeyDestroy(sessionKey);
    }

    /* destroy xmlsec objects */
#ifndef XMLSEC_NO_XMLENC
    if(encCtx != NULL) {
	xmlSecEncCtxDestroy(encCtx);
    }
#endif /* XMLSEC_NO_XMLENC */
    
#ifndef XMLSEC_NO_XMLDSIG
    if(dsigCtx != NULL) {
	xmlSecDSigCtxDestroy(dsigCtx);
    }
#endif /* XMLSEC_NO_XMLDSIG */

    if(keysMngrCtx != NULL) {
	xmlSecObjDelete(xmlSecObjCast(keysMngrCtx));
    }
    if(keysMngr != NULL) {
	xmlSecObjDelete(xmlSecObjCast(keysMngr));
    }
    
    /**
     * Shutdown xmlsec
     */
    xmlSecShutdown();
    
    /* 
     * Shutdown libxslt/libxml
     */
    #ifndef XMLSEC_NO_XSLT
    xsltCleanupGlobals();            
#endif /* XMLSEC_NO_XSLT */
    xmlCleanupParser();

    /**
     * Shutdown crypto engine
     */    
    xmlSecCryptoAppShutdown();
}

/**
 * Command line options
 */
int  readNumber(const char *str, int *number) {
    if(sscanf(str, "%d", number) <= 0) {
	fprintf(stderr, "Error: the number is expected instead of \"%s\"\n", str);
	return(-1);
    }
    return(0);
}

int  readTime(const char* str, time_t* t) {
    struct tm tm;
    int n;
    
    if((str == NULL) || (t == NULL)) {
	return(-1);
    }
    memset(&tm, 0, sizeof(tm));
    tm.tm_isdst = -1;
    
    n = sscanf(str, "%4d-%2d-%2d%*c%2d:%2d:%2d", 
	    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
	    &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    if((n != 6) || (tm.tm_year < 1900) 
		|| (tm.tm_mon  < 1) || (tm.tm_mon  > 12) 
		|| (tm.tm_mday < 1) || (tm.tm_mday > 31)
		|| (tm.tm_hour < 0) || (tm.tm_hour > 23)
		|| (tm.tm_min  < 0) || (tm.tm_min  > 59)
		|| (tm.tm_sec  < 0) || (tm.tm_sec  > 61)) {
	return(-1);	    
    }

    tm.tm_year -= 1900; /* tm relative format year */
    tm.tm_mon  -= 1; /* tm relative format month */

    (*t) = mktime(&tm);
    return(0);    
}

int  readPEMCertificate(const char *file, int trusted) {
#ifndef XMLSEC_NO_X509	    
    int ret;
/* todo
    ret = xmlSecSimpleKeysMngrLoadPemCert(xmlSecSimpleKeysMngrCast(keysMngr), 
					file, trusted);
    if(ret < 0) {
	fprintf(stderr, "Error: unable to load certificate file \"%s\".\n", file);
    	return(-1);
    }     
*/    
    return(0);
#else /* XMLSEC_NO_X509 */
    fprintf(stderr, "Error: x509 support disabled.\n");
    return(-1);
#endif /* XMLSEC_NO_X509 */    
}

int  readKeys(char *file) {
    int ret;
    
    ret = xmlSecSimpleKeysMngrLoad(xmlSecSimpleKeysMngrCast(keysMngr), file, 0);
    if(ret < 0) {
	fprintf(stderr, "Error: failed to load keys from \"%s\".\n", file);
	return(-1);
    }
    return(0);
}

int  readKeyOrigins(char *keyOrigins) {
    xmlSecKeyOrigin res = xmlSecKeyOriginDefault;
    char *p;
    
    p = strtok(keyOrigins, ",");
    while(p != NULL) {
	if(strcmp(p, "keymanager") == 0) {
	    res |= xmlSecKeyOriginKeyManager;
	} else if(strcmp(p, "keyname") == 0) {
	    res |= xmlSecKeyOriginKeyName;
	} else if(strcmp(p, "keyvalue") == 0) {
	    res |= xmlSecKeyOriginKeyValue;
	} else if(strcmp(p, "retrieval-doc") == 0) {
	    res |= xmlSecKeyOriginRetrievalDocument;
	} else if(strcmp(p, "retrieval-remote") == 0) {
	    res |= xmlSecKeyOriginRetrievalRemote;
	} else if(strcmp(p, "x509") == 0) {
	    res |= xmlSecKeyOriginX509;
	} else if(strcmp(p, "pgp") == 0) {
	    res |= xmlSecKeyOriginPGP;
	} else if(strcmp(p, "enc-key") == 0) {
	    res |= xmlSecKeyOriginEncryptedKey;
	} else {
	    fprintf(stderr, "Error: unknown key origin: \"%s\" (ignored)\n", p);
	    return(-1);
	}
	p = strtok(NULL, ",");
    }    
    if(keysMngrCtx != NULL) {
	keysMngrCtx->allowedOrigins = res;
    }  
    return(0);
}

int readPemKey(int privateKey, char *param, char *name) {
    char *p;
    xmlSecKeyPtr key;
    int ret;
    
    p = strtok(param, ","); 
    key = xmlSecOpenSSLEvpLoadPemKey(p, global_pwd, privateKey);
    if(key == NULL) {
	fprintf(stderr, "Error: failed to load key from \"%s\"\n", p);
	return(-1);
    }
    if(name != NULL) {
	key->name = xmlStrdup(BAD_CAST name);
    }

    p = strtok(NULL, ",");
#ifndef XMLSEC_NO_X509     
    while((p != NULL) && (privateKey)) {
	ret = xmlSecKeyReadPemCert(key, p);
	if(ret < 0){
	    fprintf(stderr, "Error: failed to load cert from \"%s\"\n", p);
	    xmlSecKeyDestroy(key);
	    return(-1);
	}
	p = strtok(NULL, ","); 
    }
#else /* XMLSEC_NO_X509 */
    if(p != NULL) {
	fprintf(stderr, "Error: x509 support disabled.\n");
	xmlSecKeyDestroy(key);
	return(-1);
    }
#endif /* XMLSEC_NO_X509 */        
    
    ret = xmlSecSimpleKeysMngrAddKey(xmlSecSimpleKeysMngrCast(keysMngr), key);
    if(ret < 0) {
	fprintf(stderr, "Error: failed to add key.\n");
	xmlSecKeyDestroy(key);
	return(-1);
    }

    return(0);
}

int readPKCS12Key(char *filename, char *name) {
#ifndef XMLSEC_NO_X509     
    xmlSecKeyPtr key;
    char pwd[1024] = "";
    char prompt[1024];
    int ret;
    
    if(global_pwd == NULL) {
	snprintf(prompt, sizeof(prompt), "Password for pkcs12 file \"%s\": ", filename); 
#ifdef XMLSEC_CRYPTO_OPENSSL
	ret = EVP_read_pw_string(pwd, sizeof(pwd), prompt, 0);
	if(ret != 0) {
	    fprintf(stderr, "Error: password propmpt failed for file \"%s\"\n", filename); 
	    return(-1);
	}	
#else
	/* todo: do something more intelligent and remove openssl dependenct */
	scanf("%200s", pwd);
#endif /* XMLSEC_CRYPTO_OPENSSL */
    } 

    key = xmlSecPKCS12ReadKey(filename, (global_pwd != NULL) ? global_pwd : pwd);
    if(key == NULL) {
	fprintf(stderr, "Error: failed to load pkcs12 file \"%s\"\n", filename); 
	return(-1);
    }
    if(name != NULL) {
    	key->name = xmlStrdup(BAD_CAST name);
    }
    
    ret = xmlSecSimpleKeysMngrAddKey(xmlSecSimpleKeysMngrCast(keysMngr), key);
    if(ret < 0) {
	xmlSecKeyDestroy(key);
	fprintf(stderr, "Error: failed to add hmac key\n"); 
	return(-1);
    }
    
    return(0);
#else /* XMLSEC_NO_X509 */
    fprintf(stderr, "Error: x509 support disabled.\n");
    return(-1);
#endif /* XMLSEC_NO_X509 */       
}

int readHmacKey(char *filename, char *name) {
#ifndef XMLSEC_NO_HMAC
    FILE *f;
    unsigned char buf[1024];
    xmlSecKeyPtr key;
    xmlSecKeyValuePtr keyValue;
    int ret;    
    
    f = fopen(filename, "r");
    if(f == NULL) {
	fprintf(stderr, "Error: failed to open file \"%s\" \n", filename);
	return(-1);
    }
    
    ret = fread(buf, 1, sizeof(buf), f);
    if(ret < 0) {
	fprintf(stderr, "Error: failed to read file \"%s\" \n", filename);
	fclose(f);
	return(-1);
    }
    fclose(f);    
    
    /* HMAC */    
    keyValue = xmlSecKeyValueCreate(xmlSecHmacKeyValue);
    if(keyValue == NULL) {
	fprintf(stderr, "Error: failed to create hmac key\n"); 
	return(-1);
    }    
    ret = xmlSecKeyValueSet(keyValue, buf, ret);
    if(ret < 0) {
	fprintf(stderr, "Error: failed to set key value\n"); 
	xmlSecKeyValueDestroy(keyValue);
	return(-1);
    }    

    key = xmlSecKeyCreate(keyValue, BAD_CAST name);
    if(key == NULL) {
	xmlSecKeyValueDestroy(keyValue);
	fprintf(stderr, "Error: failed to create key\n"); 
	return(-1);
    }    
    xmlSecKeyValueDestroy(keyValue);

    ret = xmlSecSimpleKeysMngrAddKey(xmlSecSimpleKeysMngrCast(keysMngr), key);
    if(ret < 0) {
	xmlSecKeyDestroy(key);
	fprintf(stderr, "Error: failed to add hmac key\n"); 
	return(-1);
    }
    return(0);
#else /* XMLSEC_NO_HMAC */
    fprintf(stderr, "Error: hmac algorithm support disabled\n"); 
    return(-1);    
#endif /* XMLSEC_NO_HMAC */
}


/**
 * XML Digital Signature
 */ 
#ifndef XMLSEC_NO_XMLDSIG
 
void getDSigResult(xmlSecDSigResultPtr result, xmlSecDSigStatusPtr status) {
    xmlSecReferenceResultPtr ref;
    
    if((result == NULL) || (status == NULL)) {
	fprintf(stderr, "Error: result or result is null\n");
	return;
    }
    
    if(result->result == xmlSecTransformStatusOk) {
	++(status->signaturesOk);
    } else {
	++(status->signaturesFail);
    }
    
    ref = result->firstSignRef;
    while(ref != NULL) {
	if(ref->result == xmlSecTransformStatusOk) {
	    ++(status->signRefOk);
	} else {
	    ++(status->signRefFail);
	}
	ref = ref->next;
    }

    ref = result->firstManifestRef;
    while(ref != NULL) {
	if(ref->result == xmlSecTransformStatusOk) {
	    ++(status->manifestRefOk);
	} else {
	    ++(status->manifestRefFail);
	}
	ref = ref->next;
    }
}

int generateDSig(xmlDocPtr doc) {    
    xmlSecDSigResultPtr result = NULL;
    xmlNodePtr signNode;
    xmlChar *string = NULL;	
    int len;
    int ret;
    int res = -1;
    clock_t start_time;

    signNode = xmlSecFindNode(xmlDocGetRootElement(doc), 
			      BAD_CAST "Signature", xmlSecDSigNs);
    if(signNode == NULL) {
        fprintf(stderr,"Error: failed to find Signature node\n");
	return(-1);
    }    

    start_time = clock();
    ret = xmlSecDSigGenerate(dsigCtx, sessionKey, signNode, &result);
    total_time += clock() - start_time;    
    if(ret < 0) {
        fprintf(stderr,"Error: xmlSecDSigGenerate() failed \n");
	goto done;    
    }
    
    if(repeats <= 1) { 
        /*
	 * Print document out in default UTF-8 encoding
         */
	xmlDocDumpMemoryEnc(doc, &string, &len, NULL);
        if(string == NULL) {
	    fprintf(stderr,"Error: failed to dump document to memory\n");
	    goto done;
        }
	
	if(output) {
	    FILE* f = fopen(output, "w");
	    if(f == NULL) {
		fprintf(stderr,"Error: failed to open output file \"%s\"\n", output);
		goto done;
	    }
	    fwrite(string, len, 1, f);
	    fclose(f);
	} else {	    
	    fwrite(string, len, 1, stdout);
	}
    }
    res = 0;
    
done:    
    if(printFile != NULL) {
	fclose(printFile);
    }
    if(string != NULL) {
	xmlFree(string);        
    }
    if(result != NULL) {
	xmlSecDSigResultDestroy(result);
    }
    return(res);
}

int validateDSig(xmlDocPtr doc) {    
    xmlSecDSigResultPtr result = NULL;
    xmlSecDSigStatus status;
    xmlNodePtr signNode;
    clock_t start_time;
    int ret;
    	    
    signNode = xmlSecFindNode(xmlDocGetRootElement(doc), 
			      BAD_CAST "Signature", xmlSecDSigNs);
    if(signNode == NULL) {
        fprintf(stderr,"Error: failed to find Signature node\n");
	return(-1);
    }    

    start_time = clock();        
    ret = xmlSecDSigValidate(dsigCtx, sessionKey, signNode, &result);
    total_time += clock() - start_time;    
    if((ret < 0) || (result == NULL)){
	fprintf(stdout,"ERROR\n");
	if(result != NULL) { 
	    xmlSecDSigResultDestroy(result); 
	}
	return(-1);
    } 
	    
    if(printResult) {
	if(printXml) {	
    	    xmlSecDSigResultDebugXmlDump(result, 
		    (printFile != NULL) ? printFile : stderr);
	} else {
    	    xmlSecDSigResultDebugDump(result, 
		    (printFile != NULL) ? printFile : stderr);
	}	
    }	
	    
    /** 
     * we will simply walk thru and calculate the number of 
     * ok/fails
     */
    memset(&status, 0, sizeof(status));
    getDSigResult(result, &status);
    
    if(repeats <= 1){ 
        
	fprintf(stderr, "= Status:\n");
	fprintf(stderr, "== Signatures ok: %d\n", status.signaturesOk);
        fprintf(stderr, "== Signatures fail: %d\n", status.signaturesFail);
	fprintf(stderr, "== SignedInfo Ref ok: %d\n", status.signRefOk);
        fprintf(stderr, "== SignedInfo Ref fail: %d\n", status.signRefFail);
	fprintf(stderr, "== Manifest Ref ok: %d\n", status.manifestRefOk);
        fprintf(stderr, "== Manifest Ref fail: %d\n", status.manifestRefFail);
    }
    	    	
    if(result != NULL) {
	xmlSecDSigResultDestroy(result);
    }

    if(status.signaturesFail == 0) {
	fprintf(stdout, "OK\n");  
	return(0);    
    }
    
    fprintf(stdout, "FAIL\n");
    return(-1);
}
#endif /* XMLSEC_NO_XMLDSIG */


/**
 * XML Encryption
 */
#ifndef XMLSEC_NO_XMLENC
int encrypt(xmlDocPtr tmpl) {    
    xmlSecEncResultPtr encResult = NULL;
    xmlChar *result = NULL;	
    xmlDocPtr doc = NULL;
    clock_t start_time;
    int len;
    int ret;
    int res = -1;

    if(binary && (data != NULL)) {
        start_time = clock();        
	ret = xmlSecEncryptUri(encCtx, sessionKey,
				xmlDocGetRootElement(tmpl), data, 
				&encResult);
        total_time += clock() - start_time;    
	if(ret < 0) {
    	    fprintf(stderr,"Error: xmlSecEncryptUri() failed \n");
	    goto done;    
	} 

    } else if(!binary && (data != NULL)) { 
	xmlNodePtr cur;
	
	/** 
	 * Load doc
	 */
	doc = xmlParseFile(data);
	if (doc == NULL) {
	    fprintf(stderr, "Error: unable to parse file \"%s\"\n", data);
	    goto done;    
	}

	/**
	 * What do we want to replace?
	 */    
	if(nodeId != NULL) {
	    xmlAttrPtr attr;
	    
	    attr = xmlGetID(doc, BAD_CAST nodeId);
	    cur = (attr != NULL) ? attr->parent : NULL;
	} else if(nodeName != NULL) {
	    cur = xmlSecFindNode(xmlDocGetRootElement(doc), BAD_CAST nodeName, BAD_CAST nodeNs);
	} else {
	    cur = xmlDocGetRootElement(doc);
	}
	
	/*
	 * Did we found node?
	 */    
	if(cur == NULL) {
    	    fprintf(stderr,"Error: empty document for file \"%s\" or unable to find node\n", data);
	    goto done;    
	}

        start_time = clock();        	
	ret = xmlSecEncryptXmlNode(encCtx, sessionKey,
				xmlDocGetRootElement(tmpl), 
				cur, &encResult);	
        total_time += clock() - start_time;    
	if(ret < 0) {
    	    fprintf(stderr,"Error: xmlSecEncryptXmlNode() failed \n");
	    goto done;    
	} 
    } else {
        fprintf(stderr,"Error: unknown type or bad type parameters\n");
	goto done;    
    }
    
    if(repeats <= 1) {	
        /*
	 * Print document out in default UTF-8 encoding
         */     
	if((encResult != NULL) && (encResult->replaced) && (doc != NULL)) {
	    xmlDocDumpMemoryEnc(doc, &result, &len, NULL);  
        } else {
    	    xmlDocDumpMemoryEnc(tmpl, &result, &len, NULL);
        }
	if(result == NULL) {
    	    fprintf(stderr,"Error: failed to dump document to memory\n");
    	    goto done;
        }
	if(output) {
	    FILE* f = fopen(output, "w");
	    if(f == NULL) {
		fprintf(stderr,"Error: failed to open output file \"%s\"\n", output);
		goto done;
	    }
	    fwrite(result, len, 1, f);
	    fclose(f);
	} else {	    
	    fwrite(result, len, 1, stdout);
	}
    }
    res = 0;

    if(printResult) {
    	xmlSecEncResultDebugDump(encResult, stderr);
    }	
        
done:    
    if(doc != NULL) {
	xmlFreeDoc(doc);
    }
    if(result != NULL) {
	xmlFree(result);        
    }
    if(encResult != NULL) {
	xmlSecEncResultDestroy(encResult);
    }
    return(res);
}

int decrypt(xmlDocPtr doc) {    
    xmlSecEncResultPtr encResult = NULL;
    xmlNodePtr cur;
    clock_t start_time;
    int ret;
    int res = -1;

    cur = xmlSecFindNode(xmlDocGetRootElement(doc), BAD_CAST "EncryptedData", xmlSecEncNs);
    if(cur == NULL) {
        fprintf(stderr,"Error: unable to find EncryptedData node\n");
	goto done;    
    }

    start_time = clock();            
    ret = xmlSecDecrypt(encCtx, sessionKey, cur, &encResult);
    total_time += clock() - start_time;    
    if(ret < 0) {
        fprintf(stderr,"Error: xmlSecDecrypt() failed \n");
	goto done;    
    } 

    if(repeats <= 1) {
	FILE* f = stdout;
	if(output) {
	    f = fopen(output, "w");
	    if(f == NULL) {
		fprintf(stderr,"Error: failed to open output file \"%s\"\n", output);
		goto done;
	    }
	}

	if((encResult != NULL) && encResult->replaced && (encResult->buffer != NULL)) {
	    ret = xmlDocDump(f, doc);    
        } else if((encResult != NULL) && !encResult->replaced) {
    	    ret = fwrite(xmlBufferContent(encResult->buffer), 
	    		 xmlBufferLength(encResult->buffer),
	        	 1, f);		       
	} else {
	    if(f != stdout) {
		fclose(f);
	    }
    	    fprintf(stderr,"Error: bad results \n");
	    goto done;    
	}
	if(f != stdout) {
	    fclose(f);
	}
        if(ret < 0) {
	    fprintf(stderr,"Error: failed to print out the result \n");
	    goto done;    
	}
    
	if(printResult) {
    	    xmlSecEncResultDebugDump(encResult, stderr);
	}
    }	
        
    res = 0;
    
done:    
    if(encResult != NULL) {
	xmlSecEncResultDestroy(encResult);
    }
    return(res);
}
#endif /* XMLSEC_NO_XMLENC */

/**
 * Keys generation/manipulation
 */
xmlSecKeyPtr genKey(const char* type, const char *name) {
    xmlSecKeyPtr key = NULL;    
    xmlSecKeyValuePtr keyValue = NULL;    

    if(type == NULL) {
    	fprintf(stderr, "Error: bad key type (NULL)\n");
	return(NULL);
    } else
#ifndef XMLSEC_NO_HMAC    
    if(strcmp(type, "hmac") == 0) {
	keyValue = xmlSecKeyValueGenerate(xmlSecHmacKeyValue, 24);
    } else 
#endif /* XMLSEC_NO_HMAC */ 
#ifndef XMLSEC_NO_RSA
    if(strcmp(type, "rsa") == 0) {
	keyValue = xmlSecKeyValueGenerate(xmlSecRsaKeyValue, 1024);	
    } else 
#endif /* XMLSEC_NO_RSA */    
#ifndef XMLSEC_NO_DSA
    if(strcmp(type, "dsa") == 0) {
	keyValue = xmlSecKeyValueGenerate(xmlSecDsaKeyValue, 1024);	
    } else 
#endif /* XMLSEC_NO_DSA */    
#ifndef XMLSEC_NO_DES
    if(strcmp(type, "des3") == 0) {
	keyValue = xmlSecKeyValueGenerate(xmlSecDesKeyValue, 24);	
    } else 
#endif /* XMLSEC_NO_DES */
#ifndef XMLSEC_NO_AES
    if(strcmp(type, "aes128") == 0) {
    	keyValue = xmlSecKeyValueGenerate(xmlSecAesKeyValue, 16);	
    } else 
    if(strcmp(type, "aes192") == 0) {
    	keyValue = xmlSecKeyValueGenerate(xmlSecAesKeyValue, 24);	
    } else 
    if(strcmp(type, "aes256") == 0) {
    	keyValue = xmlSecKeyValueGenerate(xmlSecAesKeyValue, 32);	
    } else 
#endif /* XMLSEC_NO_AES */
    {
    	fprintf(stderr, "Error: \"%s\" is a bad key identifier or the algorithm support was disabled during compilation\n", type);
	return(NULL);
    }
    
    if(keyValue == NULL) {
	fprintf(stderr, "Error: failed to create key \"%s\" \n", type);
	return(NULL);	
    }
    
    key = xmlSecKeyCreate(keyValue, BAD_CAST name);
    if(key == NULL) {
	xmlSecKeyValueDestroy(keyValue);
	fprintf(stderr, "Error: failed to create key\n");
	return(NULL);	
    }
    xmlSecKeyValueDestroy(keyValue);
    
    return(key);
}
    

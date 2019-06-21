/*-
 * Copyright (C) 2010, Romain Tartiere.
 * See AUTHORS file for a more comprehensive list of contributors.
 * Additional contributors of this file:
 * Copyright (C) 2014      Joshua Wright
 * 
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 * 
 * $Id$
 */

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <nfc/nfc.h>

#include <freefare.h>

int main(int argc, char *argv[])
{
	int error = EXIT_SUCCESS;
	nfc_device *device = NULL;
	MifareTag *tags = NULL;

	if (argc > 1)
		errx(EXIT_FAILURE, "usage: %s", argv[0]);

	nfc_connstring devices[8];
	size_t device_count;

	nfc_context *context;
	nfc_init(&context);
	if (context == NULL)
		errx(EXIT_FAILURE, "Unable to init libnfc (malloc)");

	device_count = nfc_list_devices(context, devices, 8);
	if (device_count <= 0)
		errx(EXIT_FAILURE, "No NFC device found.");

	for (size_t d = 0; d < device_count; d++) {
		device = nfc_open(context, devices[d]);
		if (!device) {
			warnx("nfc_open() failed.");
			error = EXIT_FAILURE;
			continue;
		}

		tags = freefare_get_tags(device);
		if (!tags) {
			nfc_close(device);
			errx(EXIT_FAILURE, "Error listing tags.");
		}

		for (int i = 0; (!error) && tags[i]; i++) {
			MifareTag tag = tags[i];
			if (DESFIRE != freefare_get_tag_type(tags[i]))
				continue;

			int res;
			char *tag_uid = freefare_get_tag_uid(tags[i]);

			struct mifare_desfire_version_info info;

			res = mifare_desfire_connect(tags[i]);
			if (res < 0) {
				warnx
				    ("Can't connect to Mifare DESFire target.");
				error = 1;
				break;
			}

			res = mifare_desfire_get_version(tags[i], &info);
			if (res < 0) {
				freefare_perror(tags[i],
						"mifare_desfire_get_version");
				error = 1;
				break;
			}

			printf("===> Version information for tag %s:\n",
			       tag_uid);
			printf
			    ("UID:                      0x%02x%02x%02x%02x%02x%02x%02x\n",
			     info.uid[0], info.uid[1], info.uid[2], info.uid[3],
			     info.uid[4], info.uid[5], info.uid[6]);
			printf
			    ("Batch number:             0x%02x%02x%02x%02x%02x\n",
			     info.batch_number[0], info.batch_number[1],
			     info.batch_number[2], info.batch_number[3],
			     info.batch_number[4]);
			printf("Production date:          week %x, 20%02x\n",
			       info.production_week, info.production_year);
			printf("Hardware Information:\n");
			printf("    Vendor ID:            0x%02x\n",
			       info.hardware.vendor_id);
			printf("    Type:                 0x%02x\n",
			       info.hardware.type);
			printf("    Subtype:              0x%02x\n",
			       info.hardware.subtype);
			printf("    Version:              %d.%d\n",
			       info.hardware.version_major,
			       info.hardware.version_minor);
			printf
			    ("    Storage size:         0x%02x (%s%d bytes)\n",
			     info.hardware.storage_size,
			     (info.hardware.storage_size & 1) ? ">" : "=",
			     1 << (info.hardware.storage_size >> 1));
			printf("    Protocol:             0x%02x\n",
			       info.hardware.protocol);
			printf("Software Information:\n");
			printf("    Vendor ID:            0x%02x\n",
			       info.software.vendor_id);
			printf("    Type:                 0x%02x\n",
			       info.software.type);
			printf("    Subtype:              0x%02x\n",
			       info.software.subtype);
			printf("    Version:              %d.%d\n",
			       info.software.version_major,
			       info.software.version_minor);
			printf
			    ("    Storage size:         0x%02x (%s%d bytes)\n",
			     info.software.storage_size,
			     (info.software.storage_size & 1) ? ">" : "=",
			     1 << (info.software.storage_size >> 1));
			printf("    Protocol:             0x%02x\n",
			       info.software.protocol);

			uint8_t settings;
			uint8_t max_keys;
			res =
			    mifare_desfire_get_key_settings(tags[i], &settings,
							    &max_keys);
			if (res == 0) {
				printf("Master Key settings (0x%02x):\n",
				       settings);
				printf("    0x%02x configuration changeable;\n",
				       settings & 0x08);
				printf
				    ("    0x%02x PICC Master Key not required for create / delete;\n",
				     settings & 0x04);
				printf
				    ("    0x%02x Free directory list access without PICC Master Key;\n",
				     settings & 0x02);
				printf
				    ("    0x%02x Allow changing the Master Key;\n",
				     settings & 0x01);
			} else if (AUTHENTICATION_ERROR ==
				   mifare_desfire_last_picc_error(tags[i])) {
				printf("Master Key settings: LOCKED\n");
			} else {
				freefare_perror(tags[i],
						"mifare_desfire_get_key_settings");
				error = 1;
				break;
			}

			uint8_t version;
			mifare_desfire_get_key_version(tags[i], 0, &version);
			printf("Master Key version: %d (0x%02x)\n", version,
			       version);

			uint32_t size;
			res = mifare_desfire_free_mem(tags[i], &size);
			printf("Free memory: ");
			if (0 == res) {
				printf("%d bytes\n", size);
			} else {
				printf("unknown\n");
			}

			printf("Use random UID: %s\n",
			       (strlen(tag_uid) / 2 == 4) ? "yes" : "no");

			MifareDESFireAID *aids = NULL;
			size_t aid_count;
			res =
			    mifare_desfire_get_application_ids(tag, &aids,
							       &aid_count);
			printf("AIDs enumerated: %u\n",
			       (unsigned int)aid_count);

			for (int aidindex = 0; aidindex < aid_count; aidindex++) {
				res =
				    mifare_desfire_select_application(tag,
								      aids
								      [aidindex]);
				printf("\tSelected AID 0x%x\n",
				       mifare_desfire_aid_get_aid(aids
								  [aidindex]));
				if (res < 0)
					errx(EXIT_FAILURE,
					     "Application selection failed");

				// Get all files
				uint8_t *files;
				size_t file_count;
				res =
				    mifare_desfire_get_file_ids(tag, &files,
								&file_count);
				printf("\tAID has %lu files\n",
				       (unsigned long)file_count);

				char buffer[16];
				for (size_t filenum = 0; filenum < file_count;
				     filenum++) {
					printf("\t\tFile %u ", files[filenum]);
					res =
					    mifare_desfire_read_data(tag,
								     files
								     [filenum],
								     0, 16,
								     buffer);
					if (res < 0) {
						printf
						    ("failed read (key required)\n");
						continue;
					}

					for (int filebyte = 0;
					     filebyte < (16 - 1); filebyte++) {
						printf("%02hhX:",
						       buffer[filebyte]);
					}
					printf("%02hhX", buffer[15]);
					printf("\n");
				}

				printf("\n");

			}
			free(tag_uid);

			mifare_desfire_disconnect(tags[i]);
		}

		freefare_free_tags(tags);
		nfc_close(device);
	}
	nfc_exit(context);
	exit(error);
}				/* main() */
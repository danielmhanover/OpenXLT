//
//  openxlt.cpp
//  openxlt
//
//  Created by Daniel Hanover on 7/22/13.
//  Copyright (c) 2013 klab. All rights reserved.
//
#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <ctime>
#include <stdio.h>
#include <stdlib.h>
#include "mat.h"

using namespace std;

#define SECS_PER_DAY 86400
time_t serialConvert(long double s) {
	/* convert day number to y,m,d format */
	time_t rawtime;
	struct tm * timeinfo = new tm();
	long d = s / (int) 1;
	long double p = s - d;

	// Modified Julian to DMY calculation with an addition of 2415019
	int l = d + 68569 + 2415019;
	int n = int((4 * l) / 146097);
	l = l - int((146097 * n + 3) / 4);
	int i = int((4000 * (l + 1)) / 1461001);
	l = l - int((1461 * i) / 4) + 31;
	int j = int((80 * l) / 2447);
	timeinfo->tm_mday = l - int((2447 * j) / 80);
	l = int(j / 11);
	timeinfo->tm_mon = j + 2 - (12 * l);
	timeinfo->tm_year = 100 * (n - 49) + i + l - 1900;

	rawtime = mktime(timeinfo);
	rawtime += SECS_PER_DAY*p - 3600;
	return rawtime;
}

double roundTenths(double v) {
	v = (int) (v * 10 + 0.5);
	return v / 10;
}

int fpeek(FILE *stream)
{
	int c;
	c = fgetc(stream);
	ungetc(c, stream);
	return c;
}

//Read First Sample Number from ETC File
int getSampleNumberFromETC(string path, string filename) {
	int sampleNumber;
	//Load ETC file
	FILE *etcf = fopen((path + filename + string(".etc")).c_str(), "rb");
	fseek(etcf, 356, SEEK_SET);
	fread(&sampleNumber, 4, 1, etcf);
	fclose(etcf);
	return sampleNumber;
}

#define COLUMNS 130
int main(int argc, const char * argv [])
{
	//Variables
	int samplingFrequency;
	double creationSeconds;
	int sampleNumber = 0;
	int initSampleNumber;
	string filename;
	char namec[_MAX_DIR];
	//Transfer data in EEG file to string
	_splitpath(argv[1], NULL, NULL, namec, NULL);
	std::string path(argv[1]);
	path.append("\\");
	string name(namec);
	FILE *eegf = fopen((path + name + string(".eeg")).c_str(), "rb");
	if (!eegf) {
		cout << "Failed to open EEG file for read. Terminating NOW" << endl;
		return 1;
	}
	fseek(eegf, 0, SEEK_END);
	long eegsize = ftell(eegf);
	fseek(eegf, 0, SEEK_SET);
	char *eegcontents = new char[eegsize]();
	eegcontents[0] = '\0';
	char x;
	while (!feof(eegf)) {
		if ((x = fgetc(eegf)) != '\0') {
			eegcontents[strlen(eegcontents)] = x;
		}
	}
	fclose(eegf);

	//Parse EEG data for sampling frequency and Creation Time
	std::tr1::cmatch res;
	tr1::regex rx("\\(\\.\"SamplingFreq\", ([0-9]+\\.[0-9]+)\\)");
	regex_search(eegcontents, res, rx);
	samplingFrequency = atoi(res[1].first);
	tr1::regex rx2("\\(\\.\"CreationTime\", ([0-9]+\\.[0-9]+)\\)");
	regex_search(eegcontents, res, rx2);
	creationSeconds = serialConvert(atof(res[1].first));

	delete [] eegcontents;

	//Repeatedly parse STC file for duration and file name data
	FILE *stcf = fopen((path + name + string(".stc")).c_str(), "rb");
	if (!stcf) {
		cout << "Failed to open STC file for read. Terminating NOW" << endl;
		return 1;
	}
	fseek(stcf, 408, SEEK_SET);
	char f;
	int c = 0;
	while ((f = fgetc(stcf)) != EOF) {
		//Read filename and duration from STC file
		filename = "";
		int d;
		do {
			filename += f;
		} while ((f = fgetc(stcf)) != '\x00');

		fseek(stcf, 267 - filename.length(), SEEK_CUR);
		fread(&d, 4, 1, stcf);
		double duration = d*0.000499963760375975;

		cout << "Processing " << filename << ".erd" << endl;

		//Load ERD file
		FILE *erdf = fopen((path + filename + string(".erd")).c_str(), "rb");

		//Read current sample number from ETC file
		if (sampleNumber == 0) {
			sampleNumber = getSampleNumberFromETC(path, filename);
		}
		initSampleNumber = sampleNumber;
		fseek(erdf, 8656, SEEK_SET);

		mxArray *data = mxCreateDoubleMatrix(COLUMNS, (int)(duration*2100), mxREAL);
		double *dataptr = mxGetPr(data);

		//Read data from ERD File
		char peek;
		while ((peek = fpeek(erdf)) != EOF || !feof(erdf)) {
			int sampleOffset = sampleNumber - initSampleNumber;
			if (sampleNumber + 1 % samplingFrequency == 0) {
				creationSeconds++;
			}
			//Timestamp
			dataptr[sampleOffset*COLUMNS] = creationSeconds;
			//Sample Number
			dataptr[sampleOffset*COLUMNS + 1] = sampleNumber;
			fseek(erdf, 1, SEEK_CUR);
			//Read delta voltage data from ERD file
			unsigned char groupnumrep[16] = {};
			unsigned char redoindices[16] = {};
			fread(&groupnumrep, 1, 16, erdf);
			
			for (int i = 0; i < 128; i++) {
				double dvolt;
				if (groupnumrep[i / 8] % 2 == 1) {
					//Must account for varying endianness... Ugh!
					unsigned char buf[2];
					fread(&buf, 1, 2, erdf);
					signed short volt = (buf[0]) | (buf[1] << 8);
					dvolt = volt*0.265839;
					if (volt == -1) {
						redoindices[i / 8] += (1 << i % 8);
					}
				}
				else {
					signed char volt;
					fread(&volt, 1, 1, erdf);
					dvolt = volt*0.265839;
				}
				groupnumrep[i / 8] >>= 1;
				//dvolt = roundTenths(dvolt);
				dataptr[sampleOffset*COLUMNS + i + 2] = dataptr[(sampleOffset - 1)*COLUMNS + i + 2] + dvolt;

			}
			for (size_t i = 0; i < 128; i++)
			{
				if (redoindices[i / 8] % 2 == 1) {
					int volt = 0;
					fread(&volt, 4, 1, erdf);
					double dvolt = volt*0.265839;
					//dvolt = roundTenths(dvolt);
					dataptr[sampleOffset*COLUMNS + i + 2] = dvolt;
				}
				redoindices[i / 8] >>= 1;
			}
			
			sampleNumber++;
		}

		//Close ERD file
		fclose(erdf);
		char varname[11];
		sprintf(varname, "Out%.3d.mat", c);

		//Create MATLAB File
		MATFile *pmat;
		pmat = matOpen(varname, "w");

		int status = matPutVariable(pmat, "EEGData", data);
		if(status != 0) {
			cout << "Error " << status << " writing data to MAT File. Terminating NOW";
			return 1;
		}
		mxDestroyArray(data);
		matClose(pmat);
		c++;
	}
	//Close STC File
	fclose(stcf);
	return 0;
}
